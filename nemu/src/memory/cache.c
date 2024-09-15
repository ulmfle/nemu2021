#include "cache.h"

Cache_L1 cache_l1;

static int l1_of;
static CB_L1 cl1_block[NR_CL1_BLOCK];

static int random_rep(void *_cb_pool) {
    srand((unsigned)time(0));
    return rand() % ASSOC_CL1;
}

static uint32_t cb_l1_read(void *this, uint8_t off, size_t len) {
    return (*(uint32_t *)((((CB_L1 *)this)->buf) + off)) & (((1 << (len << 3))) - 1);
}

static void cb_l1_write(void *this, uint8_t off, uint8_t *data, size_t len) {
    uint8_t *dst = (uint8_t *)(((CB_L1 *)this)->buf);
    memcpy(dst, data, len);
}

static void *check_l1_hit(void *this, hwaddr_t addr) {
    int idx;
    CB_L1 *p_cb = ((Cache_L1 *)this)->assoc[GET_CI_L1(addr)];
    for (idx = 0; idx < ASSOC_CL1; ++idx) {
        if (p_cb[idx].valid && p_cb[idx].tag == GET_CT_L1(addr)) return (void *)(p_cb + idx);
    }
    return NULL;
}

static void l1_replace(void *this, hwaddr_t addr) {
    int dst,idx,flag = 1;
    CB_L1 *p_cb, *dst_cb;
    p_cb = ((Cache_L1 *)this)->assoc[GET_CI_L1(addr)];
    for (idx = 0; idx < ASSOC_CL1; ++idx) {
        if (!p_cb[idx].valid) {dst_cb = &p_cb[idx];flag = 0;break;}
    }

    if (flag) {
        dst = random_rep(((Cache_L1 *)this)->cb_pool);
        dst_cb = &(((Cache_L1 *)this)->assoc[GET_CI_L1(addr)][dst]);
    }

    dst_cb->tag = GET_CT_L1(addr);
    dst_cb->valid = 1;
    dst_cb->write(dst_cb, 0, hwa_to_va((addr & (~CO_L1_MASK))), CB_SIZE);   //attention

    if (l1_of) l1_replace(this, (addr & (~CO_L1_MASK)) + (CO_L1_MASK + 1));
}

static uint32_t l1_read(void *this, hwaddr_t addr, size_t len, bool *hit) {
    Log("");
    uint32_t val;
    l1_of = GET_CO_L1(addr) + len - CB_SIZE + 1;
    l1_of = l1_of > 0 ? l1_of : 0;
    Log("");
    CB_L1 *cb,*cb_of = NULL;
    cb = (CB_L1 *)(((Cache_L1 *)this)->check_hit(&cache_l1, addr));
    if (l1_of) cb_of = (CB_L1 *)(((Cache_L1 *)this)->check_hit(&cache_l1, addr + len));
    Log("");
    if (((l1_of == 0) && (cb == NULL)) || ((l1_of > 0) && (cb == NULL || cb_of == NULL))) {
        *hit = false;
        return 0;
    }
    Log("");
    hwaddr_t addr_of = (addr & (~CO_L1_MASK)) + (CO_L1_MASK + 1);
    val = cb->read(cb, addr, len - l1_of);
    if (l1_of) val += (cb_of->read(cb_of, addr_of, l1_of) << ((len - l1_of) << 3));
    Log("");
    l1_of = 0;
    Log("");
    return val;
}

static void l1_write(void *this, hwaddr_t addr, uint32_t data, size_t len, bool *hit) {
    Log("");
    l1_of = GET_CO_L1(addr) + len - CB_SIZE + 1;
    l1_of = l1_of > 0 ? l1_of : 0;
    CB_L1 *cb,*cb_of = NULL;
    Log("");
    cb = (CB_L1 *)(((Cache_L1 *)this)->check_hit(this, addr));
    if (l1_of) cb_of = (CB_L1 *)(((Cache_L1 *)this)->check_hit(&cache_l1, addr + len));
    Log("");
    if (((l1_of == 0) && (cb == NULL)) || ((l1_of > 0) && (cb == NULL || cb_of == NULL))) {
        *hit = false;
        return;
    }
    Log("");
    uint8_t *of_data = ((uint8_t *)&data) + len - l1_of;
    cb->write(cb, GET_CO_L1(addr), (uint8_t *)&data, len - l1_of);
    if (l1_of) cb_of->write(cb_of, 0, of_data, l1_of);
    Log("");
    l1_of = 0;
}

static void l1_init(void *this) {
    ((Cache_L1 *)this)->cb_pool = (void *)cl1_block;
    ((Cache_L1 *)this)->check_hit = check_l1_hit;
    ((Cache_L1 *)this)->replace = l1_replace;
    ((Cache_L1 *)this)->read = l1_read;
    ((Cache_L1 *)this)->write = l1_write;
    ((Cache_L1 *)this)->assoc = (CB_L1 (*)[8])(((Cache_L1 *)this)->cb_pool);
    int idx;
    for (idx = 0; idx < NR_CL1_BLOCK; ++idx) {
        cl1_block[idx].valid = 0;
        cl1_block[idx].read = cb_l1_read;
        cl1_block[idx].write = cb_l1_write;
    }
}

void init_cache() {
    cache_l1.init = l1_init;
    cache_l1.init(&cache_l1);
}