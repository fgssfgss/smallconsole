#include "sound.h"
#include <math.h>

static unsigned long long sample_nr = 0;
static double             v         = 0.0;

void sound_write_reg (uint16_t addr, uint8_t val) {
	println("writing to reg %04x = %02x", addr, val);
}

uint8_t sound_read_reg (uint16_t addr) {
	println("reading from reg %04x = %02x", addr);
	return 0x00;
}

void sound_write_wavetable (uint16_t addr, uint8_t val) {
	println("writing to wavetable %04x = %02x", addr, val);
}

uint8_t sound_read_wavetable (uint16_t addr) {
	println("reading from wavetable %04x = %02x", addr);
	return 0x00;
}

void sound_handler (void *opaque, Sint16 *audio_buffer, int buffer_length) {
	int i = 0;
	while (i < buffer_length) {
		double time = (double) sample_nr/SOUND_FREQUENCY;
		audio_buffer[i] = (Sint16) 4000.0f*((i < (buffer_length/4)) ? 1 : 0) + 4000.0f*tan(v*2.0f*M_PI*time);
		i++;
		sample_nr++;
		v += 0.25f;

		if (v > 1000.0f) {
			v = 0.0;
		}
	}
}
