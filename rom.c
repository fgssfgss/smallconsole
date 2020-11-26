#include "rom.h"

static uint8_t *rom0; // should be size of 0x4000
static uint8_t bank;

// TODO: THIS CODE PRELIMINARY SUPPORTS MBC1 AND NOMBC ROMS, FIXME PLZ

void rom_load (uint8_t *rom) {
	rom0 = rom;
	bank = 0x1;
}

uint8_t rom_read (uint16_t addr) {
	switch (addr) {
	case 0x0000 ... 0x3fff:
		return rom0[addr];
	case 0x4000 ... 0x7fff:
		return rom0[(addr - 0x4000) + (0x4000*bank)];
	default:
		return rom0[addr];
	}
}

void rom_write (uint16_t addr, uint8_t val) {
	if (addr == 0x2000) {
		bank = val;
	}
	else {
		println("WTF??");
	}
}
