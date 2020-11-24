#include "rom.h"

static uint8_t *rom0; // should be size of 0x4000

void rom_load (uint8_t *rom) {
	rom0 = rom;
}

uint8_t rom_read (uint16_t addr) {
	return rom0[addr];
}

void rom_write (uint16_t addr, uint8_t val) {
	return;
}
