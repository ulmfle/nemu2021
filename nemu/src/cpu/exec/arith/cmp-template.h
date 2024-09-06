#include "cpu/exec/template-start.h"

#define instr cmp

static void do_execute() {
    DATA_TYPE result = op_dest->val - op_src->val;

    update_eflags_pf_zf_sf((DATA_TYPE_S)result);
	cpu.eflags.CF = ((op_dest->val >> (8*DATA_BYTE - 2)) & 1) != ((result >> (8*DATA_BYTE - 2)) & 1);
	cpu.eflags.OF = MSB((op_dest->val ^ op_src->val) & (op_dest->val ^ result));
    printf("result : %u, src = %u; reg = %d, dest = %u\n", result, op_dest->val, op_dest->reg, op_src->val);
    print_asm_template2();
}

#if DATA_BYTE == 2 || DATA_BYTE == 4
make_instr_helper(si2rm);
#endif

make_instr_helper(i2r);
make_instr_helper(i2rm);
make_instr_helper(r2rm);
make_instr_helper(rm2r);

#include "cpu/exec/template-end.h"