#include "sound.h"

#include <math.h>

enum {
	SQ1_SWEEP_NEGATE_SHIFT = 0,
	SQ1_DUTY_LENGTH_LOAD,
	SQ1_START_VOL_ENV_ADD_MODE_PERIOD,
	SQ1_FRQ_LSB,
	SQ1_TRIGGER_LEN_FRQ_MSB,

	SQ2_NOT_USED,
	SQ2_DUTY_LENGTH_LOAD,
	SQ2_START_VOL_ENV_ADD_MODE_PERIOD,
	SQ2_FRQ_LSB,
	SQ2_TRIGGER_LEN_FRQ_MSB,

	WAVE_DAC_ENABLE,
	WAVE_LENGTH_LOAD,
	WAVE_VOLUME,
	WAVE_FRQ_LSB,
	WAVE_TRIGGER_LEN_FRQ_MSB,

	NOISE_NOT_USED,
	NOISE_LENGTH_LOAD,
	NOISE_START_VOL_ENV_ADD_MODE_PERIOD,
	NOISE_CLK_SHIFT_WIDTH_DIVISOR,
	NOISE_TRIGGER_LEN,

	CTRL_VIN_L_EN_VIN_R_EN,
	CTRL_LEFT_RIGHT_ENABLE,
	CTRL_POWER_CHANNEL_STATUS,

	NOT_USED,
};

typedef struct {
	// square 1
	// square 2
	// wave
	// noise
	// control
} sound_state;

static uint8_t regs[0x1F] = {0x80, 0xBF, 0xF3, 0xFF, 0xBF, 0xFF, 0x3F, 0x00, 0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF, 0xFF, 0x00, 0x00, 0xBF, 0x77, 0xF3, 0xF1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t waveram[0x10] = {0x84, 0x40, 0x43, 0xAA, 0x2D, 0x78, 0x92, 0x3C, 0x60, 0x59, 0x59, 0xB0, 0x34, 0xB8, 0x2E, 0xDA};
static sound_state state;

void sound_write_reg (uint16_t addr, uint8_t val) {
	println("writing to reg %04x = %02x", addr, val);
	regs[addr % 0xFF10] = val;
}

uint8_t sound_read_reg (uint16_t addr) {
	println("reading from reg %04x = %02x", addr);
	return regs[addr % 0xFF10];
}

void sound_write_wavetable (uint16_t addr, uint8_t val) {
	println("writing to wavetable %04x = %02x", addr, val);
	waveram[addr % 0xFF30] = val;
}

uint8_t sound_read_wavetable (uint16_t addr) {
	println("reading from wavetable %04x = %02x", addr);
	return waveram[addr % 0xFF30];
}

const double PI2 = 6.28318530718f;
float t = 0.0f;
float freq = 440.0f;

void sound_callback(void* userdata, uint8_t* stream, int len) {
	int16_t* buffer = (int16_t*) stream;
	len /= sizeof(*buffer);

	for (int i = 0; i < len; ++i) {
		buffer[i] = sin(t) * 32000;

		t += freq * PI2 / SOUND_SAMPLE_RATE;
		if(t >= PI2)
			t -= PI2;
	}
}

static int steps = 0;
void sound_step (int cycles) {
	steps += cycles;

	if (steps > (CPU_FREQ/60.0)) {
		// here we will simulate out audio interface
		steps = 0;
	}
}
