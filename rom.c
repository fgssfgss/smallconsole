#include "rom.h"
#include "norom.h"
#include "mbc1.h"

static rom_mapper_func_t cb;

void rom_load (uint8_t *rom, uint64_t filesize, int type) {
	switch(type) {
		case 0x0:
			cb = norom_get_func();
			break;
		case 0x1:
		case 0x2:
		case 0x3:
			cb = mbc1_get_func();
			break;
		default:
			break;
	}

	cb.init(rom, filesize);

	printf("ROM %02x inited!\n", type);
}

uint8_t rom_read (uint16_t addr) {
	return cb.read(addr);
}

void rom_write (uint16_t addr, uint8_t val) {
	cb.write(addr, val);
}

