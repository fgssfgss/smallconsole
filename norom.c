#include "norom.h"

static uint8_t *rom0;
static int bank;

static void norom_init(uint8_t *rom, uint64_t filesize);
static uint8_t norom_read(uint16_t address);
static void norom_write(uint16_t address, uint8_t val);

rom_mapper_func_t norom_get_func(void) {
	rom_mapper_func_t res;
	res.init = norom_init;
	res.read = norom_read;
	res.write = norom_write;
	return res;
}

static void norom_init (uint8_t *rom, uint64_t filesize) {
	bank = 1;
	rom0 = rom;
	printl("NOROM inited!\n");
}

static uint8_t norom_read (uint16_t addr) {
	switch (addr) {
	case 0x0000 ... 0x3fff:
		return rom0[addr];
	case 0x4000 ... 0x7fff:
		return rom0[(addr - 0x4000) + (0x4000*bank)];
	default:
		return rom0[addr];
	}
}

static void norom_write (uint16_t addr, uint8_t val) {
	if (addr < 0x2000) {
		printf("RAM ENABLE WRITE");
	} else if (addr < 0x4000) {
		printf("ROM BANK SELECT");
		bank = (val & 0x1f) ? : 1;
	} else if (addr < 0x6000) {
		printf("ROM BANK UPPER SELECT");
	} else if (addr < 0x8000) {
		printf("ROM RAM SELECT MODE");
	} else {
		println("WTF??");
	}
}

