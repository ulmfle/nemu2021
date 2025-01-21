#include "cpu/exec/template-start.h"

#define instr push

#if DATA_BYTE == 1
make_helper(push_i_b) {
    int8_t val = instr_fetch(eip + 1, 1);
    cpu.esp -= 4;
    swaddr_write(cpu.esp, 4, SR_SS, (int32_t)val);

    print_asm("push" str(SUFFIX) " $0x%08x", (int32_t)val);
    return 2;
}
#endif

#if DATA_BYTE != 1
static void do_execute() {
    PUSH(op_src->val);
    print_asm_template1();
}

make_instr_helper(i);
make_instr_helper(r);
make_instr_helper(rm);
#endif

#include "cpu/exec/template-end.h"