#include "cpu/exec/helper.h"

#define instr ret

make_helper(ret) {
    uint32_t addr = swaddr_read(cpu.esp, 4, SR_SS);
    cpu.esp += 4;
    cpu.eip = addr - 1;
    print_asm("ret");
    return 1;
}

//attention
make_helper(ret_w) {
    decode_i_w(eip + 1);
    uint32_t addr = swaddr_read(cpu.esp, 4, SR_SS);
    cpu.esp += 4;
    cpu.eip = addr - 3;
    cpu.esp += op_src->val;
    print_asm("ret %s", op_src->str);
    return 3;
}