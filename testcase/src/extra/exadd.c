#include "trap.h"

int NONAME(int v) {
    return v;
}

int main() {
    int a = NONAME(1);
    //int b = NONAME(2);
    //int a = 1;
    //int b = 2;
    nemu_assert(a==1);
    //nemu_assert(b==2);
    return 0;
}