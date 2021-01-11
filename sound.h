#ifndef _SOUND_H_
#define _SOUND_H_

#include "common.h"

void sound_write_reg (uint16_t addr, uint8_t val);

uint8_t sound_read_reg (uint16_t addr);

void sound_write_wavetable (uint16_t addr, uint8_t val);

uint8_t sound_read_wavetable (uint16_t addr);

void sound_handler (void *opaque, Sint16 *audio_buffer, int buffer_length);

#endif