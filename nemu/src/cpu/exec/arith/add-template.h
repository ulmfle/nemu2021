#include "cpu/exec/template-start.h"

#define instr add

static void do_execute() {
    DATA_TYPE res = op_src->val + op_dest->val;
    OPERAND_W(op_dest, res);
    update_eflags_pf_zf_sf(res);
	cpu.eflags.CF = ((op_dest->val >> (8*DATA_BYTE - 2)) & 1) != ((res >> (8*DATA_BYTE - 2)) & 1);
    //cpu.eflags.CF = res < op_dest->val;
	cpu.eflags.OF = MSB(~(op_dest->val ^ op_src->val) & (op_dest->val ^ res));

    print_asm_template2();
}


#if DATA_BYTE == 2 || DATA_BYTE == 4
make_instr_helper(si2rm);
#endif

make_instr_helper(i2rm);
make_instr_helper(r2rm);
make_instr_helper(rm2r);

make_helper(concat(add_i2eax_, SUFFIX)) {
    int len = concat(decode_i_, SUFFIX)(eip + 1);

    DATA_TYPE result = REG(0) + op_src->val;
    REG(0) = result;
    update_eflags_pf_zf_sf((DATA_TYPE_S)result);
	cpu.eflags.CF = ((REG(0) >> (8*DATA_BYTE - 2)) & 1) != ((result >> (8*DATA_BYTE - 2)) & 1);
	cpu.eflags.OF = MSB((REG(0) ^ op_src->val) & (REG(0) ^ result));
    print_asm(str(instr) str(SUFFIX) " %s,%%%s", op_src->str, REG_NAME(0));
    return len + 1;
}

#include "cpu/exec/template-end.h"