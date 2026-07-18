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

	struct {
		int length_timer;      // counts down at 256Hz, channel shuts off at 0
		int envelope_timer;    // counts down at 64Hz
		uint8_t current_volume;// live envelope output, 0-15
		int sweep_timer;       // counts down at 128Hz
		uint16_t shadow_freq;
		bool sweep_enabled;
		float phase;           // 0..8 position within the duty waveform
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

	struct {
		int length_timer;
		int envelope_timer;
		uint8_t current_volume;
		float phase;
	} internal;
} square2;



typedef struct {
	bool enabled;
	// NR41
	uint8_t length:6;
	// NR42
	uint8_t volume:4;
	bool direction;
	uint8_t envelope_period:3;
	// NR43
	uint8_t clock_shift:4;
	bool width_mode;
	uint8_t divisor_code:3;
	// NR44
	bool length_enable;

	struct {
		int length_timer;
		int envelope_timer;
		uint8_t current_volume;
		uint16_t lfsr;   // 15-bit linear feedback shift register
		float phase;     // fractional LFSR shifts accumulated for the current sample
	} internal;
} noise;


typedef struct {
	square1 channel1;
	square2 channel2;
	// wave channel3;
	noise channel4;
	bool master_enabled;
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

// the frame sequencer clocks length/envelope/sweep at fixed sub-multiples
// of the CPU clock, independent of the audio sample rate
#define FRAME_SEQUENCER_PERIOD (CPU_FREQ / 512)
static int frame_seq_cycles = 0;
static uint8_t frame_seq_step = 0;

static void dump_channel1() {
	printf("SOUND: channel1 regs:\n sweep_period 0x%x negate %d shift 0x%x\n duty 0x%x length 0x%x\n volume 0x%x direction %d envelope_period 0x%x\n freq %d length_enable %d enabled %d\n", state.channel1.sweep_period, state.channel1.negate, state.channel1.shift, state.channel1.duty, state.channel1.length, state.channel1.volume, state.channel1.direction, state.channel1.envelope_period, state.channel1.freq, state.channel1.length_enable, state.channel1.enabled);
}

static uint16_t sweep_calc_freq() {
	uint16_t delta = state.channel1.internal.shadow_freq >> state.channel1.shift;
	uint16_t new_freq = state.channel1.negate
		? state.channel1.internal.shadow_freq - delta
		: state.channel1.internal.shadow_freq + delta;

	if (new_freq > 2047) {
		state.channel1.enabled = false;
	}

	return new_freq;
}

static void trigger_channel1() {
	bool dac_enabled = (regs[SQ1_START_VOL_ENV_ADD_MODE_PERIOD] & 0xF8) != 0;

	state.channel1.enabled = dac_enabled;

	if (state.channel1.internal.length_timer == 0) {
		state.channel1.internal.length_timer = 64;
	}

	state.channel1.internal.envelope_timer = state.channel1.envelope_period ? state.channel1.envelope_period : 8;
	state.channel1.internal.current_volume = state.channel1.volume;

	state.channel1.internal.shadow_freq = state.channel1.freq;
	state.channel1.internal.sweep_timer = state.channel1.sweep_period ? state.channel1.sweep_period : 8;
	state.channel1.internal.sweep_enabled = (state.channel1.sweep_period != 0) || (state.channel1.shift != 0);
	if (state.channel1.shift != 0) {
		sweep_calc_freq(); // overflow check only, result is discarded on trigger
	}

	state.channel1.internal.phase = 0.0f;

#ifdef DEBUG_BUILD
	dump_channel1();
#endif
}

static void trigger_channel2() {
	bool dac_enabled = (regs[SQ2_START_VOL_ENV_ADD_MODE_PERIOD] & 0xF8) != 0;

	state.channel2.enabled = dac_enabled;

	if (state.channel2.internal.length_timer == 0) {
		state.channel2.internal.length_timer = 64;
	}

	state.channel2.internal.envelope_timer = state.channel2.envelope_period ? state.channel2.envelope_period : 8;
	state.channel2.internal.current_volume = state.channel2.volume;

	state.channel2.internal.phase = 0.0f;
}

static void trigger_channel4() {
	bool dac_enabled = (regs[NOISE_START_VOL_ENV_ADD_MODE_PERIOD] & 0xF8) != 0;

	state.channel4.enabled = dac_enabled;

	if (state.channel4.internal.length_timer == 0) {
		state.channel4.internal.length_timer = 64;
	}

	state.channel4.internal.envelope_timer = state.channel4.envelope_period ? state.channel4.envelope_period : 8;
	state.channel4.internal.current_volume = state.channel4.volume;

	state.channel4.internal.lfsr = 0x7FFF;
	state.channel4.internal.phase = 0.0f;
}

static void clock_length() {
	if (state.channel1.length_enable && state.channel1.internal.length_timer > 0) {
		state.channel1.internal.length_timer--;
		if (state.channel1.internal.length_timer == 0) {
			state.channel1.enabled = false;
		}
	}

	if (state.channel2.length_enable && state.channel2.internal.length_timer > 0) {
		state.channel2.internal.length_timer--;
		if (state.channel2.internal.length_timer == 0) {
			state.channel2.enabled = false;
		}
	}

	if (state.channel4.length_enable && state.channel4.internal.length_timer > 0) {
		state.channel4.internal.length_timer--;
		if (state.channel4.internal.length_timer == 0) {
			state.channel4.enabled = false;
		}
	}
}

static void clock_envelope() {
	if (state.channel1.envelope_period != 0) {
		state.channel1.internal.envelope_timer--;
		if (state.channel1.internal.envelope_timer <= 0) {
			state.channel1.internal.envelope_timer = state.channel1.envelope_period;
			if (state.channel1.direction && state.channel1.internal.current_volume < 15) {
				state.channel1.internal.current_volume++;
			}
			else if (!state.channel1.direction && state.channel1.internal.current_volume > 0) {
				state.channel1.internal.current_volume--;
			}
		}
	}

	if (state.channel2.envelope_period != 0) {
		state.channel2.internal.envelope_timer--;
		if (state.channel2.internal.envelope_timer <= 0) {
			state.channel2.internal.envelope_timer = state.channel2.envelope_period;
			if (state.channel2.direction && state.channel2.internal.current_volume < 15) {
				state.channel2.internal.current_volume++;
			}
			else if (!state.channel2.direction && state.channel2.internal.current_volume > 0) {
				state.channel2.internal.current_volume--;
			}
		}
	}

	if (state.channel4.envelope_period != 0) {
		state.channel4.internal.envelope_timer--;
		if (state.channel4.internal.envelope_timer <= 0) {
			state.channel4.internal.envelope_timer = state.channel4.envelope_period;
			if (state.channel4.direction && state.channel4.internal.current_volume < 15) {
				state.channel4.internal.current_volume++;
			}
			else if (!state.channel4.direction && state.channel4.internal.current_volume > 0) {
				state.channel4.internal.current_volume--;
			}
		}
	}
}

// clocks the LFSR once: XOR bits 0 and 1, shift right, feed the result into
// bit 14 (and also bit 6 in 7-bit width mode). The waveform output is bit 0
// of the resulting register, inverted (read directly by the caller).
static void noise_shift_lfsr() {
	uint16_t lfsr = state.channel4.internal.lfsr;
	uint8_t xor_bit = (lfsr & 0x1) ^ ((lfsr >> 1) & 0x1);

	lfsr >>= 1;
	lfsr |= (xor_bit << 14);

	if (state.channel4.width_mode) {
		lfsr = (lfsr & ~0x40) | (xor_bit << 6);
	}

	state.channel4.internal.lfsr = lfsr;
}

static void clock_sweep() {
	if (state.channel1.internal.sweep_timer > 0) {
		state.channel1.internal.sweep_timer--;
		if (state.channel1.internal.sweep_timer == 0) {
			state.channel1.internal.sweep_timer = state.channel1.sweep_period ? state.channel1.sweep_period : 8;

			if (state.channel1.internal.sweep_enabled && state.channel1.sweep_period > 0) {
				uint16_t new_freq = sweep_calc_freq();

				if (new_freq <= 2047 && state.channel1.shift > 0) {
					state.channel1.internal.shadow_freq = new_freq;
					state.channel1.freq = new_freq;
					regs[SQ1_FRQ_LSB] = new_freq & 0xFF;
					regs[SQ1_TRIGGER_LEN_FRQ_MSB] = (regs[SQ1_TRIGGER_LEN_FRQ_MSB] & ~0x7) | ((new_freq >> 8) & 0x7);

					sweep_calc_freq(); // second overflow check, per hardware behavior
				}
			}
		}
	}
}

static void frame_sequencer_tick() {
	if ((frame_seq_step & 1) == 0) {
		clock_length();
	}
	if (frame_seq_step == 2 || frame_seq_step == 6) {
		clock_sweep();
	}
	if (frame_seq_step == 7) {
		clock_envelope();
	}

	frame_seq_step = (frame_seq_step + 1) & 0x7;
}

void sound_init() {
	state.master_enabled = true;
	state.channel1.enabled = false;
	state.channel2.enabled = false;
	state.channel4.enabled = false;
	frame_seq_cycles = 0;
	frame_seq_step = 0;
}

void sound_write_reg (uint16_t addr, uint8_t val) {
	//println("SOUND: writing to reg %04x = %02x", addr, val);

	if (!state.master_enabled && addr != 0xFF26) {
		// while the APU is powered off, writes to every register but NR52 are ignored
		return;
	}

	regs[addr - 0xFF10] = val;

	switch (addr) {
	case 0xFF10: // NR10
		state.channel1.sweep_period = (val >> 4) & 0x7;
		state.channel1.negate = (val & 0x8) != 0;
		state.channel1.shift = val & 0x7;
		break;

	case 0xFF11: // NR11
		state.channel1.duty = (val >> 6) & 0x3;
		state.channel1.length = val & 0x3F;
		state.channel1.internal.length_timer = 64 - state.channel1.length;
		break;

	case 0xFF12: // NR12
		state.channel1.volume = (val >> 4) & 0xF;
		state.channel1.direction = (val & 0x8) != 0;
		state.channel1.envelope_period = val & 0x7;
		if ((val & 0xF8) == 0) {
			state.channel1.enabled = false; // DAC off silences the channel immediately
		}
		break;

	case 0xFF13: // NR13
		state.channel1.freq = (state.channel1.freq & 0x700) | val;
		break;

	case 0xFF14: // NR14
		state.channel1.freq = (state.channel1.freq & 0xFF) | ((val & 0x7) << 8);
		state.channel1.length_enable = (val & 0x40) != 0;
		if (val & 0x80) {
			trigger_channel1();
		}
		break;

	case 0xFF16: // NR21
		state.channel2.duty = (val >> 6) & 0x3;
		state.channel2.length = val & 0x3F;
		state.channel2.internal.length_timer = 64 - state.channel2.length;
		break;

	case 0xFF17: // NR22
		state.channel2.volume = (val >> 4) & 0xF;
		state.channel2.direction = (val & 0x8) != 0;
		state.channel2.envelope_period = val & 0x7;
		if ((val & 0xF8) == 0) {
			state.channel2.enabled = false;
		}
		break;

	case 0xFF18: // NR23
		state.channel2.freq = (state.channel2.freq & 0x700) | val;
		break;

	case 0xFF19: // NR24
		state.channel2.freq = (state.channel2.freq & 0xFF) | ((val & 0x7) << 8);
		state.channel2.length_enable = (val & 0x40) != 0;
		if (val & 0x80) {
			trigger_channel2();
		}
		break;

	case 0xFF20: // NR41
		state.channel4.length = val & 0x3F;
		state.channel4.internal.length_timer = 64 - state.channel4.length;
		break;

	case 0xFF21: // NR42
		state.channel4.volume = (val >> 4) & 0xF;
		state.channel4.direction = (val & 0x8) != 0;
		state.channel4.envelope_period = val & 0x7;
		if ((val & 0xF8) == 0) {
			state.channel4.enabled = false;
		}
		break;

	case 0xFF22: // NR43
		state.channel4.clock_shift = (val >> 4) & 0xF;
		state.channel4.width_mode = (val & 0x8) != 0;
		state.channel4.divisor_code = val & 0x7;
		break;

	case 0xFF23: // NR44
		state.channel4.length_enable = (val & 0x40) != 0;
		if (val & 0x80) {
			trigger_channel4();
		}
		break;

	case 0xFF26: // NR52
		state.master_enabled = (val & 0x80) != 0;
		if (!state.master_enabled) {
			state.channel1.enabled = false;
			state.channel2.enabled = false;
			state.channel4.enabled = false;
		}
		break;

	default:
		// wave channel is stored in regs[] but not mixed into audio output yet
		break;
	}
}

uint8_t sound_read_reg (uint16_t addr) {
	//println("SOUND: reading from reg %04x = %02x", addr);

	if (addr == 0xFF26) {
		uint8_t val = 0x70; // unused bits always read back as 1
		val |= state.master_enabled ? 0x80 : 0x00;
		val |= state.channel1.enabled ? 0x01 : 0x00;
		val |= state.channel2.enabled ? 0x02 : 0x00;
		val |= state.channel4.enabled ? 0x08 : 0x00;
		return val;
	}

	return regs[addr - 0xFF10];
}

void sound_write_wavetable (uint16_t addr, uint8_t val) {
	//println("SOUND: writing to wavetable %04x = %02x", addr, val);
	waveram[addr % 0xFF30] = val;
}

uint8_t sound_read_wavetable (uint16_t addr) {
	//println("SOUND: reading from wavetable %04x = %02x", addr);
	return waveram[addr % 0xFF30];
}

void sound_step (int cycles) {
	frame_seq_cycles += cycles;

	while (frame_seq_cycles >= FRAME_SEQUENCER_PERIOD) {
		frame_seq_cycles -= FRAME_SEQUENCER_PERIOD;
		frame_sequencer_tick();
	}
}

void sound_callback(void* userdata, uint8_t* stream, int len) {
	int16_t* buffer = (int16_t*) stream;
	len /= sizeof(*buffer);

	uint8_t nr50 = regs[CTRL_VIN_L_EN_VIN_R_EN];
	uint8_t nr51 = regs[CTRL_LEFT_RIGHT_ENABLE];
	float left_vol = (nr50 >> 4) & 0x7;
	float right_vol = nr50 & 0x7;
	float master_vol = ((left_vol + right_vol) / 2.0f + 1.0f) / 8.0f;

	// NR51: bit0/4 = channel1 right/left, bit1/5 = channel2 right/left, bit3/7 = channel4 right/left
	bool ch1_audible = state.master_enabled && state.channel1.enabled && (nr51 & 0x11);
	bool ch2_audible = state.master_enabled && state.channel2.enabled && (nr51 & 0x22);
	bool ch4_audible = state.master_enabled && state.channel4.enabled && (nr51 & 0x88);

	// tone frequency in Hz = CPU_FREQ / (32 * (2048 - freq)); the duty
	// waveform has 8 steps, so it advances 8x that rate per second
	float ch1_step = 8.0f * (CPU_FREQ / 32.0f) / (2048 - state.channel1.freq) / SOUND_SAMPLE_RATE;
	float ch2_step = 8.0f * (CPU_FREQ / 32.0f) / (2048 - state.channel2.freq) / SOUND_SAMPLE_RATE;

	// LFSR shifts once per (divisor << clock_shift) CPU cycles
	int ch4_period = divisor[state.channel4.divisor_code] << state.channel4.clock_shift;
	float ch4_step = (CPU_FREQ / (float) ch4_period) / SOUND_SAMPLE_RATE;

	for (int i = 0; i < len; ++i) {
		float sample = 0.0f;

		if (ch1_audible) {
			int duty_idx = ((int) state.channel1.internal.phase) & 0x7;
			float digital = duties[state.channel1.duty][duty_idx] ? state.channel1.internal.current_volume : 0.0f;
			sample += digital - 7.5f;
		}
		state.channel1.internal.phase += ch1_step;
		if (state.channel1.internal.phase >= 8.0f) {
			state.channel1.internal.phase -= 8.0f;
		}

		if (ch2_audible) {
			int duty_idx = ((int) state.channel2.internal.phase) & 0x7;
			float digital = duties[state.channel2.duty][duty_idx] ? state.channel2.internal.current_volume : 0.0f;
			sample += digital - 7.5f;
		}
		state.channel2.internal.phase += ch2_step;
		if (state.channel2.internal.phase >= 8.0f) {
			state.channel2.internal.phase -= 8.0f;
		}

		state.channel4.internal.phase += ch4_step;
		while (state.channel4.internal.phase >= 1.0f) {
			state.channel4.internal.phase -= 1.0f;
			noise_shift_lfsr();
		}
		if (ch4_audible) {
			float digital = (state.channel4.internal.lfsr & 0x1) ? 0.0f : state.channel4.internal.current_volume;
			sample += digital - 7.5f;
		}

		sample *= master_vol * 2000.0f;

		if (sample > 32000.0f) sample = 32000.0f;
		if (sample < -32000.0f) sample = -32000.0f;

		buffer[i] = (int16_t) sample;
	}
}
