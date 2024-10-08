#include "cpu/exec/helper.h"

void load_desc(uint8_t, uint16_t);
lnaddr_t seg_translate(swaddr_t, uint8_t);

#define DATA_BYTE 1
#include "jmp-template.h"
#undef DATA_BYTE

#define DATA_BYTE 4
#include "jmp-template.h"
#undef DATA_BYTE

make_helper(ljmp_wl) {
    int len = decode_i_l(eip + 1);
    uint32_t r = op_src->val;
    len += decode_i_w(eip + len + 1);
    uint16_t l = op_src->val;

    load_desc(SR_CS, l);
    cpu.eip = seg_translate(r, SR_CS) - (len + 1);

    print_asm("ljmp $%#x,$%#x", l, r);
    return len + 1;
}