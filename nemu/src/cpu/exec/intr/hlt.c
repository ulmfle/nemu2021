#include "device/i8259.h"
#include "intr.h"
#include "cpu/exec/helper.h"

make_helper(hlt) {
    uint32_t intr_NO = 0;
    print_asm("hlt");
    while (!(cpu.INTR & cpu.eflags.IF))
        intr_NO = i8259_query_intr();
    cpu.eip += 1;
    i8259_ack_intr();
    raise_intr(intr_NO);
    return 0;
}