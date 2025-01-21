#include "common.h"
#include "memory.h"
#include <string.h>
#include <elf.h>

#define ELF_OFFSET_IN_DISK 0

#ifdef HAS_DEVICE
void ide_read(uint8_t *, uint32_t, uint32_t);
#else
void ramdisk_read(uint8_t *, uint32_t, uint32_t);
#endif

#define STACK_SIZE (1 << 20)

void create_video_mapping();
uint32_t get_ucr3();

uint32_t loader() {
	Elf32_Ehdr *elf;
	Elf32_Phdr *ph = NULL;

	uint8_t buf[4096];

#ifdef HAS_DEVICE
	ide_read(buf, ELF_OFFSET_IN_DISK, 4096);
#else
	ramdisk_read(buf, ELF_OFFSET_IN_DISK, 4096);
#endif

	elf = (void*)buf;

	/* TODO: fix the magic number with the correct one */
	const uint32_t elf_magic = 0x464c457f;
	uint32_t *p_magic = (void *)buf;
	nemu_assert(*p_magic == elf_magic);

	/* Load each program segment */
	int nr_ph = elf->e_phnum;
	ph = (Elf32_Phdr *)((void *)buf + elf->e_phoff);

	int ph_idx;
	for (ph_idx = 0; ph_idx < nr_ph; ++ph_idx) {
		/* Scan the program header table, load each segment into memory */
#ifndef HAS_DEVICE
		if(ph[ph_idx].p_type != PT_LOAD) continue;
#endif
		/* TODO: read the content of the segment from the ELF file
			* to the memory region [VirtAddr, VirtAddr + FileSiz)
			*/
		uint8_t *pmalloc = (uint8_t *)mm_malloc(ph[ph_idx].p_vaddr, ph[ph_idx].p_memsz);

#ifdef HAS_DEVICE
		ide_read(pmalloc, ELF_OFFSET_IN_DISK + ph[ph_idx].p_offset, ph[ph_idx].p_filesz);
#else
		ramdisk_read(pmalloc, ELF_OFFSET_IN_DISK + ph[ph_idx].p_offset, ph[ph_idx].p_filesz);
#endif
		/* TODO: zero the memory region
		* [VirtAddr + FileSiz, VirtAddr + MemSiz)
		*/
		memset((void *)(pmalloc + ph[ph_idx].p_filesz), 0, ph[ph_idx].p_memsz - ph[ph_idx].p_filesz);

#ifdef IA32_PAGE
		/* Record the program break for future use. */
		if(ph[ph_idx].p_type == PT_LOAD) {
			extern uint32_t cur_brk, max_brk;
			uint32_t new_brk = ph[ph_idx].p_vaddr + ph[ph_idx].p_memsz - 1;
			if(cur_brk < new_brk) { max_brk = cur_brk = new_brk; }
		}
#endif
	}

	volatile uint32_t entry = elf->e_entry;

#ifdef IA32_PAGE
	mm_malloc(KOFFSET - STACK_SIZE, STACK_SIZE);

#ifdef HAS_DEVICE
	create_video_mapping();
#endif

	write_cr3(get_ucr3());
#endif

	return entry;
}
