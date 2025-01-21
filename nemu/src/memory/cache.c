#include "common.h"
#include "cpu/reg.h"
#include "memory/memory.h"
#include <stdlib.h>
#include <time.h>       //for random

#define CB_SIZE_WIDTH 6
#define NR_CL1_BLOCK_WIDTH 10
#define NR_CL2_BLOCK_WIDTH 12
#define NR_TLBE_WIDTH 6
#define ASSOC_CL1_WIDTH 3
#define ASSOC_CL2_WIDTH 4
#define TAG_CL1_WIDTH (32 - CB_SIZE_WIDTH - NR_CL1_BLOCK_WIDTH + ASSOC_CL1_WIDTH)
#define TAG_CL2_WIDTH (32 - CB_SIZE_WIDTH - NR_CL2_BLOCK_WIDTH + ASSOC_CL2_WIDTH)
#define TAG_WIDTH(level) concat3(TAG_CL, level, _WIDTH)

#define CB_SIZE (1 << CB_SIZE_WIDTH)
#define NR_CL1_BLOCK (1 << NR_CL1_BLOCK_WIDTH)
#define NR_CL2_BLOCK (1 << NR_CL2_BLOCK_WIDTH)
#define NR_TLBE (1 << NR_TLBE_WIDTH)
#define ASSOC_CL1 (1 << ASSOC_CL1_WIDTH)
#define ASSOC_CL2 (1 << ASSOC_CL2_WIDTH)

#define CT_MASK(level) (~0u << (32 - TAG_WIDTH(level)))
#define CO_MASK (CB_SIZE - 1)
#define CI_MASK(level) (((~0u) ^ (CT_MASK(level))) ^ (CO_MASK))
#define TLB_TAG_MASK (~0u << 12)

#define GET_CT(addr, level) (((addr) & CT_MASK(level)) >> (32 - TAG_WIDTH(level)))
#define GET_CI(addr, level) (((addr) & CI_MASK(level)) >> CB_SIZE_WIDTH)
#define GET_CO(addr) ((addr) & CO_MASK)
#define GET_TLB_TAG(addr) (((addr) & TLB_TAG_MASK) >> 12)

#define ASSOC(level) ((CB (*)[concat(ASSOC_CL, level)])(this->cb_pool))

typedef struct CB {
    void *buf;

    uint32_t tag;
    bool valid;
    bool dirty;
    uint32_t (*read)(struct CB *, uint8_t, size_t);
    void (*write)(struct CB *, uint8_t, uint8_t *, size_t);
} CB;

typedef struct Cache {
    void *cb_pool;

    CB *(*check_read_hit)(struct Cache *, hwaddr_t);
    CB *(*check_write_hit)(struct Cache *, hwaddr_t);
    void (*read_replace)(struct Cache *, hwaddr_t);
    void (*write_replace)(struct Cache *, hwaddr_t);
    uint32_t (*read)(struct Cache *, hwaddr_t, size_t, bool *);
    void (*write)(struct Cache *, hwaddr_t, uint32_t, size_t, bool *);
} Cache;

static uint8_t l1_buf[NR_CL1_BLOCK][CB_SIZE];
static uint8_t l2_buf[NR_CL2_BLOCK][CB_SIZE];
static uint8_t tlb_buf[NR_TLBE][sizeof(hwaddr_t)];
static CB l1_block[NR_CL1_BLOCK];
static CB l2_block[NR_CL2_BLOCK];
static CB tlb_entry[NR_TLBE];
Cache l1, l2;

//stand-alone
static CB *normal_check_hit(CB *cb_lst, size_t len, uint32_t _tag) {
    int idx;
    for (idx = 0; idx < len; ++idx) {
        if (cb_lst[idx].valid && cb_lst[idx].tag == _tag)
            return (cb_lst + idx);
    }
    return NULL;
}

//stand-alone
static CB *normal_find_replace(CB *cb_lst, size_t len) {
    CB *dst_cb = NULL;
    int idx;
    for (idx = 0; idx < len; ++idx) {
        if (cb_lst[idx].valid) continue;
        dst_cb = cb_lst + idx;
        break;
    }
    if (dst_cb == NULL) {
        srand((unsigned)time(NULL));
        dst_cb = cb_lst + (rand() % len);
    }
    return dst_cb;
}

//stand-alone
static CB *find_and_writeback(CB *cb_lst, uint32_t addr, size_t len, size_t tag_width) {
    CB *dst_cb = NULL;

    int idx;
    for (idx = 0; idx < len; ++idx) {
        if (cb_lst[idx].valid && cb_lst[idx].dirty) {
            dst_cb = cb_lst + idx;
            break;
        }
    }

    if (dst_cb == NULL) dst_cb = normal_find_replace(cb_lst, len);

    if (dst_cb != NULL && dst_cb->dirty) {
        //write back
        memcpy(hwa_to_va((((addr & (~(~0u << (32 - tag_width)))) ^ (dst_cb->tag << (32 - tag_width))) & (~CO_MASK))), dst_cb->buf, CB_SIZE);
        memset(dst_cb->buf, 0, CB_SIZE);
        dst_cb->dirty = 0;
    }

    return dst_cb;
}

//base
static uint32_t cbread(CB *this, uint8_t off, size_t len) {
    return (*(uint32_t *)(this->buf + off)) & (~0u >> ((4 - len) << 3));
}

//base
static void cbwrite(CB *this, uint8_t off, uint8_t *data, size_t len) {
    memcpy(this->buf + off, data, len);
}

//base
static uint32_t cread(Cache *this, hwaddr_t addr, size_t len, bool *hit) {
    CB *cb = this->check_read_hit(this, addr);
    if (cb == NULL) {
        *hit = false;
        return 0;
    }
    *hit = true;
    return cb->read(cb, GET_CO(addr), len);
}

//base
static void cwrite(Cache *this, hwaddr_t addr, uint32_t data, size_t len, bool *hit) {
    CB *cb = this->check_write_hit(this, addr);
    if (cb == NULL) {
        *hit = false;
        return;
    }
    *hit = true;
    cb->write(cb, GET_CO(addr), (uint8_t *)&data, len);
}

static CB *l1_check_hit(Cache *this, hwaddr_t addr) {
    return normal_check_hit(ASSOC(1)[GET_CI(addr, 1)], ASSOC_CL1, GET_CT(addr, 1));
}

static CB *l2_check_read_hit(Cache *this, hwaddr_t addr) {
    return normal_check_hit(ASSOC(2)[GET_CI(addr, 2)], ASSOC_CL2, GET_CT(addr, 2));
}

static CB *l2_check_write_hit(Cache *this, hwaddr_t addr) {
    CB *ret = normal_check_hit(ASSOC(2)[GET_CI(addr, 2)], ASSOC_CL2, GET_CT(addr, 2));
    if (ret != NULL) ret->dirty = 1;
    return ret;
}

static void l1_replace(Cache *this, hwaddr_t addr) {
    CB *dst_cb = normal_find_replace(ASSOC(1)[GET_CI(addr, 1)], ASSOC_CL1);
    CB *src_cb = l2.check_read_hit(&l2, addr);
    dst_cb->tag = GET_CT(addr, 1);
    dst_cb->valid = 1;
    dst_cb->write(dst_cb, 0, src_cb->buf, CB_SIZE);
}

static void l2_replace(Cache *this, hwaddr_t addr) {
    CB *dst_cb = find_and_writeback(ASSOC(2)[GET_CI(addr, 2)], addr, ASSOC_CL2, TAG_WIDTH(2));
    dst_cb->tag = GET_CT(addr, 2);
    dst_cb->valid = 1;
    dst_cb->write(dst_cb, 0, hwa_to_va((addr - GET_CO(addr))), CB_SIZE);
}

static CB *tlb_check_read_hit(lnaddr_t addr) {
    return normal_check_hit(tlb_entry, NR_TLBE, GET_TLB_TAG(addr));
}

static void tlb_read_replace(lnaddr_t addr, hwaddr_t res) {
    CB *dst_cb = normal_find_replace(tlb_entry, NR_TLBE);
    dst_cb->tag = GET_TLB_TAG(addr);
    dst_cb->valid = 1;
    dst_cb->write(dst_cb, 0, (uint8_t *)&res, 4);
}

//main
static void init_cache_internal() {
    l1.cb_pool = (void *)l1_block;
    l2.cb_pool = (void *)l2_block;
    l1.read = l2.read = cread;
    l1.write = l2.write = cwrite;
    l1.check_read_hit = l1.check_write_hit =  l1_check_hit;
    l2.check_read_hit = l2_check_read_hit;
    l2.check_write_hit = l2_check_write_hit;
    l1.read_replace = l1.write_replace = l1_replace;
    l2.read_replace = l2.write_replace = l2_replace;

    memset(l1_buf, 0, NR_CL1_BLOCK * CB_SIZE);
    memset(l2_buf, 0, NR_CL2_BLOCK * CB_SIZE);
    memset(tlb_buf, 0, NR_TLBE * sizeof(hwaddr_t));

    int l1_idx;
    for (l1_idx = 0; l1_idx < NR_CL1_BLOCK; ++l1_idx) {
        l1_block[l1_idx].buf = l1_buf[l1_idx];
        l1_block[l1_idx].read = cbread;
        l1_block[l1_idx].write = cbwrite;
        l1_block[l1_idx].valid = 0;
    }

    int l2_idx;
    for (l2_idx = 0; l2_idx < NR_CL2_BLOCK; ++l2_idx) {
        l2_block[l2_idx].buf = l2_buf[l2_idx];
        l2_block[l2_idx].read = cbread;
        l2_block[l2_idx].write = cbwrite;
        l2_block[l2_idx].valid = 0;
        l2_block[l2_idx].dirty = 0;
    }

    int tlb_idx;
    for (tlb_idx = 0; tlb_idx < NR_TLBE; ++tlb_idx) {
        tlb_entry[tlb_idx].buf = tlb_buf[tlb_idx];
        tlb_entry[tlb_idx].read = cbread;
        tlb_entry[tlb_idx].write = cbwrite;
        tlb_entry[tlb_idx].valid = 0;
    }
}

//main
void init_cache() {
    init_cache_internal();
}

//main
uint32_t cache_read(hwaddr_t addr, size_t len, bool *hit) {
    uint32_t val = 0;

    int of = GET_CO(addr) + len - CB_SIZE;
    if (of > 0) {
        bool hit_l, hit_r;
        val += cache_read(addr, len - of, &hit_l);
        val += cache_read(addr + len - GET_CO(addr + len), of, &hit_r) << ((len - of) << 3);
        *hit = hit_l && hit_r;
        return val;
    }

    bool hit_l1, hit_l2;
    val = l1.read(&l1, addr, len, &hit_l1);
    if (hit_l1 == 0) {
        val = l2.read(&l2, addr, len, &hit_l2);
    } else {
        *hit = true;
        return val;
    }
    if (hit_l2 != 0) {
        l1.read_replace(&l1, addr);
        *hit = true;
        return val;
    } else {
        l2.read_replace(&l2, addr);
        *hit = false;
        return 0;
    }
}

//main
void cache_write(hwaddr_t addr, uint32_t data, size_t len) {
    int of = GET_CO(addr) + len - CB_SIZE;
    if (of > 0) {
        cache_write(addr, data, len - of);
        cache_write(addr + len - GET_CO(addr + len), data >> ((len - of) << 3), of);
        return;
    }

    bool hit_l1, hit_l2;
    l1.write(&l1, addr, data, len, &hit_l1);   //write through
    l2.write(&l2, addr, data, len, &hit_l2);
    if (hit_l2 == false) {
        l2.write_replace(&l2, addr);
        l2.write(&l2, addr, data, len, &hit_l2);   //write allocate (move to L2 and write again)
    }
}

//main
uint32_t tlb_read(lnaddr_t addr, bool *hit) {
    CB *dst_cb = tlb_check_read_hit(addr);
    if (dst_cb == NULL) {
        *hit = false;
        return 0;
    }
    *hit = true;
    return dst_cb->read(dst_cb, 0, 4);
}

//main
void tlb_replace(lnaddr_t addr, uint32_t res) {
    tlb_read_replace(addr, res);
}

//main
int tlb_flush() {
    static CR3 prev_cr3;
    int tlb_idx;

    if (cpu.cr3.val != prev_cr3.val) {
        memset(tlb_buf, 0, sizeof(uint32_t)*NR_TLBE);
        prev_cr3.val = cpu.cr3.val;
        for (tlb_idx = 0; tlb_idx < NR_TLBE; ++tlb_idx) {
            tlb_entry[tlb_idx].valid = 0;
        }
        return true;
    }
    return false;
}