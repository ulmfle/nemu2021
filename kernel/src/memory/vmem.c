#include "common.h"
#include "memory.h"
#include <string.h>

#define VMEM_ADDR 0xa0000
#define SCR_SIZE (320 * 200)

/* Use the function to get the start address of user page directory. */
PDE* get_updir();
PTE* get_kptable();

void create_video_mapping() {
	/* TODO: create an identical mapping from virtual memory area 
	 * [0xa0000, 0xa0000 + SCR_SIZE) to physical memory area 
	 * [0xa0000, 0xa0000 + SCR_SIZE) for user program. You may define
	 * some page tables to create this mapping.
	 */
	PDE *upd = get_updir();
	PTE *kpt = va_to_pa(get_kptable()) + NR_PTE * VMEM_ADDR / PT_SIZE;
	upd[VMEM_ADDR / PT_SIZE].val = make_pde(kpt);
}

void video_mapping_write_test() {
	int i;
	uint8_t *buf = (void *)VMEM_ADDR;
	for(i = 0; i < SCR_SIZE; i ++) {
		buf[i] = 0x22;
	}
}

void video_mapping_read_test() {
	int i;
	uint8_t *buf = (void *)VMEM_ADDR;
	for(i = 0; i < SCR_SIZE; i ++) {
		assert(buf[i] == 0x22);
	}
}

void video_mapping_clear() {
	memset((void *)VMEM_ADDR, 0, SCR_SIZE);
}
