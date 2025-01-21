#include "cpu/exec/helper.h"

make_helper(cbw) {
    cpu.gpr[R_EAX]._16 = (int16_t)(cpu.gpr[R_EAX]._8[0]);

    print_asm_template1();
    return 1;
}

make_helper(cwde) {
    cpu.gpr[R_EAX]._32 = (int32_t)(cpu.gpr[R_EAX]._16);
    
    print_asm_template1();
    return 1;
}

#define cvt_w cbw
#define cvt_l cwde

make_helper_v(cvt);

#undef cvt_w
#undef cvt_l
