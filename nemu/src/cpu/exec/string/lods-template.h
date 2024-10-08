#include "cpu/exec/template-start.h"

#define instr lods

make_helper(concat(lods_, SUFFIX)) {
    REG(R_EAX) = MEM_R(SR_DS, cpu.esi);
    cpu.esi += (cpu.eflags.DF ? -DATA_BYTE : DATA_BYTE);

    print_asm("lods" str(SUFFIX) " %%ds:(%%esi),%%eax");
    return 1;
}

#include "cpu/exec/template-end.h"
