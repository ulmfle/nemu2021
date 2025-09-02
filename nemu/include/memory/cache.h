#ifndef __CACHE_H__
#define __CACHE_H__

#include "common.h"
#include <stdlib.h>

typedef struct Caches {
    struct {
        uint32_t (*read)(hwaddr_t, size_t);
        void (*write)(hwaddr_t, uint32_t, size_t);
        void (*refresh)();
    } std;
    struct {
        uint32_t (*read)(lnaddr_t, bool *);
        void (*replace)(lnaddr_t, uint32_t);
        int (*flush)();
    } tlb;
} Caches;

extern const Caches caches;

#endif
