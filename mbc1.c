#include "norom.h"

static uint8_t *memory;
static uint8_t ram[0x8000]; // just statically allocate it, maybe change in the future
static int rom_bank;
static int ram_bank;
static int mode;
static bool ram_enabled; 

static void mbc1_init(uint8_t *rom, uint64_t filesize);
static uint8_t mbc1_read(uint16_t address);
static void mbc1_write(uint16_t address, uint8_t val);

rom_mapper_func_t mbc1_get_func(void) {
	rom_mapper_func_t res;
	res.init = mbc1_init;
	res.read = mbc1_read;
	res.write = mbc1_write;
	return res;
}

static void mbc1_init(uint8_t *rom, uint64_t filesize) {
	rom_bank = 1;
	ram_bank = 0;
	mode = 0;
	ram_enabled = false;
	memory = rom;
	memset(ram, 0x00, sizeof(ram));
	printl("MBC1 mapper inited!\n");
}

static uint8_t mbc1_read(uint16_t addr) {
	switch (addr) {
	case 0x0000 ... 0x3fff:
		return memory[addr];
	case 0x4000 ... 0x7fff:
		return memory[(addr - 0x4000) + (0x4000*rom_bank)];
	case 0xa000 ... 0xbfff:
		return ram[(addr - 0xa000) + (0x4000*ram_bank)];
	}
}

static void mbc1_write(uint16_t addr, uint8_t val) {
	switch (addr) {
	case 0x0000 ... 0x1fff: {
		ram_enabled = (val & 0x0a) ? true : false;	
	}
	break;
	case 0x2000 ... 0x3fff: {
		uint8_t mask = (mode) ? 0 : 0xe0;
		val &= 0x1f;
		if (val == 0) {
			val = 1;
		}
		rom_bank = (rom_bank & mask) | val;
	} 
	break;
	case 0x4000 ... 0x5fff: {
		val &= 0x3;
		if (mode == 0) {
			rom_bank = (rom_bank & 0x1f) | (val << 5);
		} else {
			ram_bank = val;
		}
	}
	break;
	case 0x6000 ... 0x7fff: {
		mode = val & 0x1;
	}
	break;
	case 0xa000 ... 0xbfff: {
		ram[(addr - 0xa000) + (0x4000 * ram_bank)] = val;
	}
	break;
	}
}

