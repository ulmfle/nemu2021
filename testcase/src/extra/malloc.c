#include "trap.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    void *p = malloc(100);
    printf("\n%p\n",p);
    nemu_assert(p);
    return 0;
}