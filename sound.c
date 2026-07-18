#include "sound.h"
#include <math.h>

typedef struct {
	bool enabled;
	// NR10
	uint8_t sweep_period:3;
	bool negate;
	uint8_t shift:3;
	// NR11
	uint8_t duty:2;
	uint8_t length:6;
	// NR12
	uint8_t volume:4;
	bool direction;
	uint8_t envelope_period:3;
	// NR13 & NR14
	uint16_t freq;
	bool length_enable;
	bool trigger;

	struct {

	} internal;
} square1;


typedef struct {
	bool enabled;
	// NR20 unused
	// NR21
	uint8_t duty:2;
	uint8_t length:6;
	// NR12
	uint8_t volume:4;
	bool direction;
	uint8_t envelope_period:3;
	// NR13 & NR14
	uint16_t freq;
	bool length_enable;
	bool trigger;

	struct {

	} internal;
} square2;



typedef struct {
	square1 channel1;
	square2 channel2;
	// wave channel3;
	// noise channel4;
	// control ???
} sound_state;

static uint8_t regs[0x1F] = {0x80, 0xBF, 0xF3, 0xFF, 0xBF, 0xFF, 0x3F, 0x00, 0xFF, 0xBF, 0x7F, 0xFF, 0x9F, 0xFF, 0xBF, 0xFF, 0xFF, 0x00, 0x00, 0xBF, 0x77, 0xF3, 0xF1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t waveram[0x10] = {0x84, 0x40, 0x43, 0xAA, 0x2D, 0x78, 0x92, 0x3C, 0x60, 0x59, 0x59, 0xB0, 0x34, 0xB8, 0x2E, 0xDA};
static uint8_t duties[4][8] = {
		{0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 1, 1, 1},
		{0, 1, 1, 1, 1, 1, 1, 0},
};
static uint8_t divisor[8] = {8, 16, 32, 48, 64, 80, 96, 112};
static sound_state state;

static void init_channel1() {
	state.channel1.sweep_period = (regs[SQ1_SWEEP_NEGATE_SHIFT] >> 4) & 0xf;
	state.channel1.negate = (regs[SQ1_SWEEP_NEGATE_SHIFT] & 0x8) ? 1 : 0;
	state.channel1.shift = regs[SQ1_SWEEP_NEGATE_SHIFT] & 0x7;

	state.channel1.duty = (regs[SQ1_DUTY_LENGTH_LOAD] >> 6) & 0x3;
	state.channel1.length = regs[SQ1_DUTY_LENGTH_LOAD] & 0x3f;

	state.channel1.volume = (regs[SQ1_START_VOL_ENV_ADD_MODE_PERIOD] >> 4) & 0xf;
	state.channel1.direction = (regs[SQ1_START_VOL_ENV_ADD_MODE_PERIOD] & 0x8) ? 1 : 0;
	state.channel1.envelope_period = regs[SQ1_START_VOL_ENV_ADD_MODE_PERIOD] & 0x7;

	state.channel1.freq = (regs[SQ1_TRIGGER_LEN_FRQ_MSB] & 0x7) << 8 | regs[SQ1_FRQ_LSB];
	state.channel1.length_enable = (regs[SQ1_TRIGGER_LEN_FRQ_MSB] & 0x40) ? 1 : 0;
	state.channel1.trigger = (regs[SQ1_TRIGGER_LEN_FRQ_MSB] & 0x80) ? 1 : 0;
}

static void init_channel2() {
	state.channel2.duty = (regs[SQ2_DUTY_LENGTH_LOAD] >> 6) & 0x3;
	state.channel2.length = regs[SQ2_DUTY_LENGTH_LOAD] & 0x3f;

	state.channel2.volume = (regs[SQ2_START_VOL_ENV_ADD_MODE_PERIOD] >> 4) & 0xf;
	state.channel2.direction = (regs[SQ2_START_VOL_ENV_ADD_MODE_PERIOD] & 0x8) ? 1 : 0;
	state.channel2.envelope_period = regs[SQ2_START_VOL_ENV_ADD_MODE_PERIOD] & 0x7;

	state.channel2.freq = (regs[SQ2_TRIGGER_LEN_FRQ_MSB] & 0x7) << 8 | regs[SQ1_FRQ_LSB];
	state.channel2.length_enable = (regs[SQ2_TRIGGER_LEN_FRQ_MSB] & 0x40) ? 1 : 0;
	state.channel2.trigger = (regs[SQ2_TRIGGER_LEN_FRQ_MSB] & 0x80) ? 1 : 0;
}

static void reset_channel1() {
	state.channel1.trigger = false;
	regs[SQ1_TRIGGER_LEN_FRQ_MSB] &= ~0x80;

	// set envlope initial volume
	// enable more stuff internally
	state.channel1.enabled = true;
	if (state.channel1.length_enable) {
		// copy length to internal register
	}
}

static void reset_channel2() {
	state.channel2.trigger = false;
	regs[SQ2_TRIGGER_LEN_FRQ_MSB] &= ~0x80;

	// set envlope initial volume
	// enable more stuff internally
	state.channel2.enabled = true;
	if (state.channel2.length_enable) {
		// copy length to internal register
	}
}

static void dump_channel1() {
	printf("SOUND: channel1 regs:\n sweep_period 0x%x negate %d shift 0x%x\n duty 0x%x length 0x%x\n volume 0x%x direction %d envelope_period 0x%x\n freq %d length_enable %d trigger %d\n", state.channel1.sweep_period, state.channel1.negate, state.channel1.shift, state.channel1.duty, state.channel1.length, state.channel1.volume, state.channel1.direction, state.channel1.envelope_period, state.channel1.freq, state.channel1.length_enable, state.channel1.trigger);
}








void sound_write_reg (uint16_t addr, uint8_t val) {
	//println("SOUND: writing to reg %04x = %02x", addr, val);
	regs[addr % 0xFF10] = val;
}

uint8_t sound_read_reg (uint16_t addr) {
	//println("SOUND: reading from reg %04x = %02x", addr);
	return regs[addr % 0xFF10];
}

void sound_write_wavetable (uint16_t addr, uint8_t val) {
	//println("SOUND: writing to wavetable %04x = %02x", addr, val);
	waveram[addr % 0xFF30] = val;
}

uint8_t sound_read_wavetable (uint16_t addr) {
	//println("SOUND: reading from wavetable %04x = %02x", addr);
	return waveram[addr % 0xFF30];
}

const double PI2 = 6.28318530718f;
float t = 0.0f;
float t2 = 0.0f;
float t3 = 0.0f;
float freq = 391.0f;
float freq2 = 493.0f;
float freq3 = 587.0f;

void sound_callback(void* userdata, uint8_t* stream, int len) {
	int16_t* buffer = (int16_t*) stream;
	len /= sizeof(*buffer);

	memset(buffer, 0x00, len * sizeof(*buffer));

	for (int i = 0; i < len; ++i) {
		buffer[i] = sin(t) * 10000 + sin(t2) * 10000 + sin(t3) * 10000;

		t += freq * PI2 / SOUND_SAMPLE_RATE;
		if(t >= PI2)
			t -= PI2;

		t2 += freq2 * PI2 / SOUND_SAMPLE_RATE;
		if(t2 >= PI2)
			t2 -= PI2;

		t3 += freq3 * PI2 / SOUND_SAMPLE_RATE;
		if(t3 >= PI2)
			t3 -= PI2;
	}
}

static int steps = 0;
static int flag = 0;
void sound_step (int cycles) {
	steps += cycles;

	if (steps > (CPU_FREQ/60.0)) {
		init_channel1();
		reset_channel1();
		dump_channel1();
		// here we will simulate out audio interface
		flag++;
		freq = (flag % 2) ? 391.0f : 440.0f;
		freq2 = (flag % 2) ? 493.0f : 523.0f;
		freq3 = (flag % 2) ? 587.0f : 659.0f;
		steps = 0;
	}
}
