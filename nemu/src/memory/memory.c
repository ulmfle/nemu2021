#include "common.h"
#include "cpu/reg.h"
#include "memory/memory.h"
#include "device/mmio.h"

uint32_t cache_read(hwaddr_t, size_t, bool *);
void cache_replace(hwaddr_t, size_t);
void cache_write(hwaddr_t, uint32_t, size_t);
uint32_t tlb_read(lnaddr_t, bool *);
void tlb_replace(lnaddr_t, uint32_t);
int tlb_flush();
uint32_t dram_read(hwaddr_t, size_t);
void dram_write(hwaddr_t, size_t, uint32_t);
lnaddr_t seg_translate(swaddr_t, uint8_t);
hwaddr_t page_translate(lnaddr_t);
void load_desc(uint8_t, uint16_t);
void lnread64(lnaddr_t, void *);
/* Memory accessing interfaces */

uint32_t hwaddr_read(hwaddr_t addr, size_t len) {
#ifdef HAS_DEVICE
	int map;
	if ((map = is_mmio(addr)) != -1) {
		return mmio_read(addr, len, map) & (~0u >> ((4 - len) << 3));
	}
#endif
	uint32_t val;
	bool cache_hit;
	val = cache_read(addr, len, &cache_hit);

	if (cache_hit == false) {
		val = cache_read(addr, len, &cache_hit);
		//val = dram_read(addr, len) & (~0u >> ((4 - len) << 3));
	}
	return val;
}

void hwaddr_write(hwaddr_t addr, size_t len, uint32_t data) {
#ifdef HAS_DEVICE
	int map;
	if ((map = is_mmio(addr)) != -1) {
		mmio_write(addr, len, data, map);
	}
#endif
	// dram_write(addr, len, data);
	cache_write(addr, data, len);
}

uint32_t lnaddr_read(lnaddr_t addr, size_t len) {
	if ((addr + len - 1) % PAGE_SIZE < addr % PAGE_SIZE) {
		uint32_t val = 0;
		int idx;
		for (idx = len - 1; idx >= 0; --idx) {
			val += hwaddr_read(page_translate(addr + idx), 1);
			if (idx != 0) val <<= 8;
		}
		return val;
	}
	return hwaddr_read(page_translate(addr), len);
}

void lnaddr_write(lnaddr_t addr, size_t len, uint32_t data) {
	if ((addr + len - 1) % PAGE_SIZE < addr % PAGE_SIZE) {
		int idx;
		for (idx = 0; idx <= len - 1; ++idx) {
			hwaddr_write(page_translate(addr + idx), 1, data);
			data >>= 8;
		}
		return;
	}
	hwaddr_write(page_translate(addr), len, data);
}

uint32_t swaddr_read(swaddr_t addr, size_t len, uint8_t sreg) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	return lnaddr_read(seg_translate(addr, sreg), len);
}

void swaddr_write(swaddr_t addr, size_t len, uint8_t sreg, uint32_t data) {
#ifdef DEBUG
	assert(len == 1 || len == 2 || len == 4);
#endif
	lnaddr_write(seg_translate(addr, sreg), len, data);
}

lnaddr_t seg_translate(swaddr_t addr, uint8_t sreg) {
	return cpu.cr0.protect_enable ? sr_base(sreg) + addr : addr;
}

hwaddr_t page_translate(lnaddr_t addr) {
	if (!(cpu.cr0.protect_enable && cpu.cr0.paging)) return addr;
	bool hit = 0;
	PDE pde;
	PTE pte;
	pte.val = tlb_read(addr, &hit);
	int f_ret = tlb_flush();
	if (hit == true && f_ret == false) return (pte.page_frame << 12) + (addr & PAGE_MASK);

	pde.val = hwaddr_read((cpu.cr3.page_directory_base << 12) + sizeof(uint32_t)*(addr >> 22), sizeof(uint32_t)); 
	assert(pde.present);
	pte.val = hwaddr_read((pde.page_frame << 12) + sizeof(uint32_t)*((addr >> 12) & ~(~0u << 10)), sizeof(uint32_t)); //attention
	assert(pte.present);
	tlb_replace(addr, pte.val);
	return (pte.page_frame << 12) + (addr & PAGE_MASK);
}

void load_desc(uint8_t sreg, uint16_t _sel) {
	cpu.sr[sreg].sel.val = _sel;
	lnread64(cpu.gdtr.LBA + sizeof(SegDesc)*cpu.sr[sreg].sel.index, &cpu.sr[sreg].hid_desc);
}

void lnread64(lnaddr_t addr, void *dst) {
    uint32_t lv = lnaddr_read(addr, 4);
	uint32_t rv = lnaddr_read(addr + 4, 4);
    memcpy(dst, (void *)&lv, sizeof(uint32_t));
	memcpy(dst + 4, (void *)&rv, sizeof(uint32_t));
}