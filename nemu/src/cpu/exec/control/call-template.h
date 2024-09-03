#include "cpu/exec/template-start.h"

#define instr call

make_helper(concat(call_rel_,SUFFIX)) {
    int len = decode_si_l(eip + 1);

    REG(R_ESP) -= 4;
    MEM_W(REG(R_ESP), eip);

    cpu.eip += op_src->simm;

    print_asm_template1();
    return len + 1;
}

#include "cpu/exec/template-end.h"
