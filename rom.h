#ifndef _ROM_H
#define _ROM_H

#include "common.h"

void rom_load (uint8_t *rom, uint64_t filesize, int type);

uint8_t rom_read (uint16_t addr);

void rom_write (uint16_t addr, uint8_t val);

#endif //_ROM_H
