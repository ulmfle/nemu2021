#include "intr.h"

void push32(uint32_t *src) {
    cpu.esp -= 4;
    swaddr_write(cpu.esp, 4, SR_SS, *src);
}

void pop32(uint32_t *dst) {
    *dst = swaddr_read(cpu.esp, 4, SR_SS);
    cpu.esp += 4;
}

void push64(uint64_t *src) {
    push32(&(((uint32_t *)src)[1]));
    push32(&(((uint32_t *)src)[0]));
}

void pop64(uint64_t *dst) {
    pop32(&(((uint32_t *)dst)[0]));
    pop32(&(((uint32_t *)dst)[1]));
}

void raise_intr(uint8_t NO) {
    if (!cpu.eflags.IF) return;
    GateDesc gd;
	_lptr lptr;
	lptr.ptr = cpu.eip;
	lptr.sel = cpu.cs.sel.val;

    lnread64(cpu.idtr.LBA + sizeof(GateDesc)*NO, &gd);
	push32(&cpu.eflags.val);
	push64((uint64_t *)&lptr);
    load_desc(SR_CS, gd.segment);
    cpu.eip = gd.offset_15_0 + (gd.offset_31_16 << 16);
    if (!(gd.type & 1)) cpu.eflags.IF = 0;
    cpu.eflags.TF = 0;
    cpu.eflags.NT = 0;

    longjmp(jbuf,1);
}