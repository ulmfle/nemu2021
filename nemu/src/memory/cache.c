#include "common.h"
#include "cpu/reg.h"
#include "memory/cache.h"
#include "memory/memory.h"
#include <stdlib.h>
#include <time.h>       //for random

uint32_t cache_read(hwaddr_t, size_t);
void cache_write(hwaddr_t, uint32_t, size_t);
void cache_all_refresh();
uint32_t tlb_read(lnaddr_t, bool *);
void tlb_replace(lnaddr_t, uint32_t);
int tlb_flush();

#define SUM_WIDTH (sizeof(hwaddr_t) << 3)
#define CB_SIZE_WIDTH 6
#define NR_CL1_BLOCK_WIDTH 10
#define NR_CL2_BLOCK_WIDTH 12
#define NR_TLBE_WIDTH 6
#define ASSOC_CL1_WIDTH 3
#define ASSOC_CL2_WIDTH 4
#define TAG_CL1_WIDTH (SUM_WIDTH - CB_SIZE_WIDTH - NR_CL1_BLOCK_WIDTH + ASSOC_CL1_WIDTH)
#define TAG_CL2_WIDTH (SUM_WIDTH - CB_SIZE_WIDTH - NR_CL2_BLOCK_WIDTH + ASSOC_CL2_WIDTH)
#define TAG_WIDTH(level) concat3(TAG_CL, level, _WIDTH)

#define CB_SIZE (1 << CB_SIZE_WIDTH)
#define NR_CL1_BLOCK (1 << NR_CL1_BLOCK_WIDTH)
#define NR_CL2_BLOCK (1 << NR_CL2_BLOCK_WIDTH)
#define NR_TLBE (1 << NR_TLBE_WIDTH)
#define ASSOC_CL1 (1 << ASSOC_CL1_WIDTH)
#define ASSOC_CL2 (1 << ASSOC_CL2_WIDTH)

#define CT_MASK(level) (~0u << (SUM_WIDTH - TAG_WIDTH(level)))
#define CO_MASK (CB_SIZE - 1)
#define CI_MASK(level) (((~0u) ^ (CT_MASK(level))) ^ (CO_MASK))

#define GET_CT(level, addr) (((addr) & CT_MASK(level)) >> (SUM_WIDTH - TAG_WIDTH(level)))
#define GET_CI(level, addr) (((addr) & CI_MASK(level)) >> CB_SIZE_WIDTH)
#define GET_CO(addr) ((addr) & CO_MASK)
#define GET_TLB_TAG(addr) (((addr) & (~0u << 12)) >> 12)

#define ASSOC(level, poolp) ((CB (*)[concat(ASSOC_CL, level)])(poolp))

typedef struct CacheBlock {
    void *buf;
    uint32_t tag;
    bool valid;
    bool dirty;
} CB;

struct CacheBlockFunc {
    uint32_t (*read)(CB *, uint8_t, size_t);
    void (*write)(CB *, uint8_t, uint8_t *, size_t);
} block;

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
const Caches caches = {
    {cache_read, cache_write, cache_all_refresh}, {tlb_read, tlb_replace, tlb_flush}
};

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
    return block.read(cb, GET_CO(addr), len);
}

//base
static void cwrite(Cache *this, hwaddr_t addr, uint32_t data, size_t len, bool *hit) {
    CB *cb = this->check_write_hit(this, addr);
    if (cb == NULL) {
        *hit = false;
        return;
    }
    *hit = true;
    block.write(cb, GET_CO(addr), (uint8_t *)&data, len);
}

static CB *l1_check_hit(Cache *this, hwaddr_t addr) {
    return normal_check_hit(ASSOC(1, this->cb_pool)[GET_CI(1, addr)], ASSOC_CL1, GET_CT(1, addr));
}

static CB *l2_check_read_hit(Cache *this, hwaddr_t addr) {
    return normal_check_hit(ASSOC(2, this->cb_pool)[GET_CI(2, addr)], ASSOC_CL2, GET_CT(2, addr));
}

static CB *l2_check_write_hit(Cache *this, hwaddr_t addr) {
    CB *ret = normal_check_hit(ASSOC(2, this->cb_pool)[GET_CI(2, addr)], ASSOC_CL2, GET_CT(2, addr));
    if (ret != NULL) ret->dirty = 1;
    return ret;
}

static void l1_replace(Cache *this, hwaddr_t addr) {
    CB *dst_cb = normal_find_replace(ASSOC(1, this->cb_pool)[GET_CI(1, addr)], ASSOC_CL1);
    CB *src_cb = l2.check_read_hit(&l2, addr);
    dst_cb->tag = GET_CT(1, addr);
    dst_cb->valid = 1;
    block.write(dst_cb, 0, src_cb->buf, CB_SIZE);
}

static void l2_replace(Cache *this, hwaddr_t addr) {
    CB *dst_cb = find_and_writeback(ASSOC(2, this->cb_pool)[GET_CI(2, addr)], addr, ASSOC_CL2, TAG_WIDTH(2));
    dst_cb->tag = GET_CT(2, addr);
    dst_cb->valid = 1;
    block.write(dst_cb, 0, hwa_to_va((addr - GET_CO(addr))), CB_SIZE);
}

static CB *tlb_check_read_hit(lnaddr_t addr) {
    return normal_check_hit(tlb_entry, NR_TLBE, GET_TLB_TAG(addr));
}

static void tlb_read_replace(lnaddr_t addr, hwaddr_t res) {
    CB *dst_cb = normal_find_replace(tlb_entry, NR_TLBE);
    dst_cb->tag = GET_TLB_TAG(addr);
    dst_cb->valid = 1;
    block.write(dst_cb, 0, (uint8_t *)&res, 4);
}

//main
static void init_cache_internal() {
    l1.cb_pool = &l1_block;
    l2.cb_pool = &l2_block;
    l1.read = l2.read = cread;
    l1.write = l2.write = cwrite;
    l1.check_read_hit = l1.check_write_hit =  l1_check_hit;
    l2.check_read_hit = l2_check_read_hit;
    l2.check_write_hit = l2_check_write_hit;
    l1.read_replace = l1.write_replace = l1_replace;
    l2.read_replace = l2.write_replace = l2_replace;
    block.read = cbread;
    block.write = cbwrite;

    int l1_idx;
    for (l1_idx = 0; l1_idx < NR_CL1_BLOCK; ++l1_idx) {
        l1_block[l1_idx].buf = l1_buf[l1_idx];
        l1_block[l1_idx].valid = 0;
    }

    int l2_idx;
    for (l2_idx = 0; l2_idx < NR_CL2_BLOCK; ++l2_idx) {
        l2_block[l2_idx].buf = l2_buf[l2_idx];
        l2_block[l2_idx].valid = 0;
        l2_block[l2_idx].dirty = 0;
    }

    int tlb_idx;
    for (tlb_idx = 0; tlb_idx < NR_TLBE; ++tlb_idx) {
        tlb_entry[tlb_idx].buf = tlb_buf[tlb_idx];
        tlb_entry[tlb_idx].valid = 0;
    }
}

//main
void init_cache() {
    init_cache_internal();
}

//main
uint32_t cache_read(hwaddr_t addr, size_t len) {
    uint32_t val = 0;

    int of = GET_CO(addr) + len - CB_SIZE;
    if (of > 0) {
        val += cache_read(addr, len - of);
        val += cache_read(addr + len - GET_CO(addr + len), of) << ((len - of) << 3);
        return val;
    }

    bool hit_l1, hit_l2;
    val = l1.read(&l1, addr, len, &hit_l1);
    if (hit_l1 == 0) {
        val = l2.read(&l2, addr, len, &hit_l2);
    } else {
        return val;
    }
    if (hit_l2 != 0) {
        l1.read_replace(&l1, addr);
        return val;
    }
    l2.read_replace(&l2, addr);
    return cache_read(addr, len);
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
void cache_all_refresh() {
    int idx, jdx;
    for (idx = 0; idx < NR_CL1_BLOCK; idx++) {
        l1_block[idx].valid = 0;
    }
    for (idx = 0; idx < (1 << (SUM_WIDTH - TAG_WIDTH(2) - CB_SIZE_WIDTH)); idx++) {
        for (jdx = 0; jdx < ASSOC_CL2; jdx++) {
            CB *src = &ASSOC(2, &l2_block)[idx][jdx];
            if (src->valid && src->dirty) {
                uint32_t ptr = (src->tag << (SUM_WIDTH - TAG_WIDTH(2))) + (idx << CB_SIZE_WIDTH);
                memcpy(&hw_mem[ptr], src->buf, CB_SIZE);
            }
            src->valid = 0;
        }
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
    return block.read(dst_cb, 0, sizeof(hwaddr_t));
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