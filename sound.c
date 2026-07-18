#include "sound.h"
#include <math.h>
#include <stdatomic.h>

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
	bool enabled;
	// NR30
	bool dac_power;
	// NR31
	uint8_t length; // full 8-bit load value (256-L)
	// NR32
	uint8_t volume_code:2;
	// NR33 & NR34
	uint16_t freq;
	bool length_enable;

	struct {
		int length_timer;    // counts down at 256Hz, up to 256
		uint8_t position;    // 0-31, index into the 32 4-bit samples
		uint8_t sample_buffer;
		float phase;
	} internal;
} wave;


typedef struct {
	square1 channel1;
	square2 channel2;
	wave channel3;
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
static uint8_t wave_shift[4] = {4, 0, 1, 2}; // volume code -> right-shift amount (4 = silent)
static sound_state state;

// wave RAM holds 32 4-bit samples packed two per byte, high nibble first
static uint8_t read_wave_sample(uint8_t position) {
	uint8_t byte = waveram[position / 2];
	return (position % 2 == 0) ? (byte >> 4) & 0xF : byte & 0xF;
}

// the frame sequencer clocks length/envelope/sweep at fixed sub-multiples
// of the CPU clock, independent of the audio sample rate
#define FRAME_SEQUENCER_PERIOD (CPU_FREQ / 512)
static int frame_seq_cycles = 0;
static uint8_t frame_seq_step = 0;

// audio samples are synthesized cycle-accurately as the CPU thread runs
// (see generate_sample()/sound_step below) and handed to the SDL audio
// thread through this lock-free single-producer/single-consumer ring
// buffer, instead of the audio thread re-synthesizing waveforms from a
// live snapshot of register state whenever it happens to be scheduled.
// A snapshot-based approach can miss or misrepresent register writes that
// are set and undone faster than an SDL audio buffer (~21ms) turns over.
// sized generously (rather than tightly around the ~21ms SDL requests) since
// the producer runs in a burst once per emulated video frame (all of a
// frame's samples are generated back-to-back inside render_frame, then the
// CPU thread goes idle for the rest of the frame via SDL_Delay) while the
// consumer drains steadily in real time; a small buffer underruns during
// that idle stretch, or during any frame that takes longer than 1/60s to
// emulate (e.g. heavier work during a scene transition)
#define RING_BUFFER_SIZE 16384 // ~340ms of headroom at 48kHz
static int16_t ring_buffer[RING_BUFFER_SIZE];
static atomic_size_t ring_write_pos = 0;
static atomic_size_t ring_read_pos = 0;
static int16_t ring_last_sample = 0;

#define CYCLES_PER_SAMPLE ((float) CPU_FREQ / SOUND_SAMPLE_RATE)
static float sample_cycle_acc = 0.0f;

#ifdef DEBUG_BUILD
// debug/diagnostic counters, see sound_log_frame_debug(). The _generated/
// _dropped/_triggers counters are only ever touched on the CPU thread; the
// _consumed/_underrun ones are only ever touched on the audio thread, so
// they need atomics since sound_log_frame_debug() (called from the CPU
// thread) reads and resets them.
static uint32_t debug_samples_generated = 0;
static uint32_t debug_samples_dropped = 0;
static uint32_t debug_triggers[4] = {0, 0, 0, 0};
static atomic_uint debug_samples_consumed = 0;
static atomic_uint debug_underrun_samples = 0;
#endif

// post-mix filtering: naive (non-band-limited) square/noise/wave synthesis
// produces harsh edges whose harmonics alias badly once several channels are
// summed together, which is what makes a busy mix sound "choppy"/crackly. A
// gentle low-pass smooths those edges, and a DC-blocking high-pass keeps the
// signal centered so loudness doesn't drift as the number of active channels
// (and therefore the DC bias contributed by our per-channel centering) changes.
#define LOWPASS_CUTOFF_HZ  12000.0f
#define HIGHPASS_CUTOFF_HZ 20.0f
static float lowpass_state = 0.0f;
static float highpass_prev_in = 0.0f;
static float highpass_prev_out = 0.0f;

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
#ifdef DEBUG_BUILD
	debug_triggers[0]++;
#endif

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
#ifdef DEBUG_BUILD
	debug_triggers[1]++;
#endif

	bool dac_enabled = (regs[SQ2_START_VOL_ENV_ADD_MODE_PERIOD] & 0xF8) != 0;

	state.channel2.enabled = dac_enabled;

	if (state.channel2.internal.length_timer == 0) {
		state.channel2.internal.length_timer = 64;
	}

	state.channel2.internal.envelope_timer = state.channel2.envelope_period ? state.channel2.envelope_period : 8;
	state.channel2.internal.current_volume = state.channel2.volume;

	state.channel2.internal.phase = 0.0f;
}

static void trigger_channel3() {
#ifdef DEBUG_BUILD
	debug_triggers[2]++;
#endif

	state.channel3.enabled = state.channel3.dac_power;

	if (state.channel3.internal.length_timer == 0) {
		state.channel3.internal.length_timer = 256;
	}

	state.channel3.internal.position = 0;
	state.channel3.internal.sample_buffer = read_wave_sample(0);
	state.channel3.internal.phase = 0.0f;
}

static void trigger_channel4() {
#ifdef DEBUG_BUILD
	debug_triggers[3]++;
#endif

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

	if (state.channel3.length_enable && state.channel3.internal.length_timer > 0) {
		state.channel3.internal.length_timer--;
		if (state.channel3.internal.length_timer == 0) {
			state.channel3.enabled = false;
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
	state.channel3.enabled = false;
	state.channel4.enabled = false;
	frame_seq_cycles = 0;
	frame_seq_step = 0;

	sample_cycle_acc = 0.0f;
	atomic_store_explicit(&ring_write_pos, 0, memory_order_relaxed);
	atomic_store_explicit(&ring_read_pos, 0, memory_order_relaxed);
	ring_last_sample = 0;

	lowpass_state = 0.0f;
	highpass_prev_in = 0.0f;
	highpass_prev_out = 0.0f;
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

	case 0xFF1A: // NR30
		state.channel3.dac_power = (val & 0x80) != 0;
		if (!state.channel3.dac_power) {
			state.channel3.enabled = false;
		}
		break;

	case 0xFF1B: // NR31
		state.channel3.length = val;
		state.channel3.internal.length_timer = 256 - state.channel3.length;
		break;

	case 0xFF1C: // NR32
		state.channel3.volume_code = (val >> 5) & 0x3;
		break;

	case 0xFF1D: // NR33
		state.channel3.freq = (state.channel3.freq & 0x700) | val;
		break;

	case 0xFF1E: // NR34
		state.channel3.freq = (state.channel3.freq & 0xFF) | ((val & 0x7) << 8);
		state.channel3.length_enable = (val & 0x40) != 0;
		if (val & 0x80) {
			trigger_channel3();
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
			state.channel3.enabled = false;
			state.channel4.enabled = false;
		}
		break;

	default:
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
		val |= state.channel3.enabled ? 0x04 : 0x00;
		val |= state.channel4.enabled ? 0x08 : 0x00;
		return val;
	}

	return regs[addr - 0xFF10];
}

void sound_write_wavetable (uint16_t addr, uint8_t val) {
	//println("SOUND: writing to wavetable %04x = %02x", addr, val);

	if (state.channel3.enabled) {
		// wave RAM can only be reliably written while the wave channel is
		// stopped; while it's playing, the CPU and the channel's own sample
		// fetch are racing for the same bytes, so writes are dropped instead
		// of splicing new data into the note that's currently sounding
		return;
	}

	waveram[addr % 0xFF30] = val;
}

uint8_t sound_read_wavetable (uint16_t addr) {
	//println("SOUND: reading from wavetable %04x = %02x", addr);

	if (state.channel3.enabled) {
		return 0xFF;
	}

	return waveram[addr % 0xFF30];
}

// synthesizes exactly one output sample from the channels' *current*
// register-derived state, and advances each channel's waveform position by
// one sample's worth. Called from sound_step, interleaved with the CPU's
// register writes as they actually happen, so a channel that gets triggered
// and silenced again within a single audio buffer's time is represented
// correctly instead of being missed or smeared across the whole buffer.
static int16_t generate_sample() {
	uint8_t nr50 = regs[CTRL_VIN_L_EN_VIN_R_EN];
	uint8_t nr51 = regs[CTRL_LEFT_RIGHT_ENABLE];
	float left_vol = (nr50 >> 4) & 0x7;
	float right_vol = nr50 & 0x7;
	float master_vol = ((left_vol + right_vol) / 2.0f + 1.0f) / 8.0f;

	// NR51: bit0/4 = channel1 right/left, bit1/5 = channel2 right/left,
	// bit2/6 = channel3 right/left, bit3/7 = channel4 right/left
	bool ch1_audible = state.master_enabled && state.channel1.enabled && (nr51 & 0x11);
	bool ch2_audible = state.master_enabled && state.channel2.enabled && (nr51 & 0x22);
	bool ch3_audible = state.master_enabled && state.channel3.enabled && (nr51 & 0x44);
	bool ch4_audible = state.master_enabled && state.channel4.enabled && (nr51 & 0x88);

	// tone frequency in Hz = CPU_FREQ / (32 * (2048 - freq)); the duty
	// waveform has 8 steps, so it advances 8x that rate per second
	float ch1_step = 8.0f * (CPU_FREQ / 32.0f) / (2048 - state.channel1.freq) / SOUND_SAMPLE_RATE;
	float ch2_step = 8.0f * (CPU_FREQ / 32.0f) / (2048 - state.channel2.freq) / SOUND_SAMPLE_RATE;

	// wave channel advances one wave-table sample every (2048-freq)*2 cycles
	int ch3_period = (2048 - state.channel3.freq) * 2;
	float ch3_step = (CPU_FREQ / (float) ch3_period) / SOUND_SAMPLE_RATE;

	// LFSR shifts once per (divisor << clock_shift) CPU cycles
	int ch4_period = divisor[state.channel4.divisor_code] << state.channel4.clock_shift;
	float ch4_step = (CPU_FREQ / (float) ch4_period) / SOUND_SAMPLE_RATE;

	const float PI = 3.14159265358979f;
	float dt = 1.0f / SOUND_SAMPLE_RATE;
	float lp_rc = 1.0f / (2.0f * PI * LOWPASS_CUTOFF_HZ);
	float lp_alpha = dt / (lp_rc + dt);
	float hp_rc = 1.0f / (2.0f * PI * HIGHPASS_CUTOFF_HZ);
	float hp_alpha = hp_rc / (hp_rc + dt);

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

	state.channel3.internal.phase += ch3_step;
	while (state.channel3.internal.phase >= 1.0f) {
		state.channel3.internal.phase -= 1.0f;
		state.channel3.internal.position = (state.channel3.internal.position + 1) & 0x1F;
		state.channel3.internal.sample_buffer = read_wave_sample(state.channel3.internal.position);
	}
	if (ch3_audible) {
		uint8_t wshift = wave_shift[state.channel3.volume_code];
		float digital = (wshift == 4) ? 0.0f : (float) (state.channel3.internal.sample_buffer >> wshift);
		sample += digital - 7.5f;
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

	// smooth the naive digital edges (anti-aliasing)
	lowpass_state += lp_alpha * (sample - lowpass_state);
	// during silence this decays exponentially toward 0 and can linger in
	// denormal-float range for thousands of samples, where the FPU falls
	// back to slow microcode (10-100x normal cost); this now runs inline on
	// the CPU thread (see sound_step), so that slowdown steals real time
	// from CPU/GPU emulation too, not just from the audio thread's budget.
	// Snapping to exact 0 once inaudible avoids ever entering that range.
	if (fabsf(lowpass_state) < 1e-15f) {
		lowpass_state = 0.0f;
	}
	float filtered = lowpass_state;

	// block DC so loudness stays consistent regardless of how many
	// channels are contributing to the per-channel centering
	float hp_out = filtered - highpass_prev_in + hp_alpha * highpass_prev_out;
	if (fabsf(hp_out) < 1e-15f) {
		hp_out = 0.0f;
	}
	highpass_prev_in = filtered;
	highpass_prev_out = hp_out;

	float scaled = hp_out * master_vol * 2600.0f;

	// soft-knee limiter: compresses peaks smoothly instead of hard
	// clipping into a crackly flat-top when several channels stack up
	float limited = tanhf(scaled / 32000.0f) * 32000.0f;

	return (int16_t) limited;
}

// producer side: called only from the CPU thread (via sound_step)
static void ring_push(int16_t sample) {
	size_t write_pos = atomic_load_explicit(&ring_write_pos, memory_order_relaxed);
	size_t next = (write_pos + 1) % RING_BUFFER_SIZE;

	if (next == atomic_load_explicit(&ring_read_pos, memory_order_acquire)) {
		// the audio thread has fallen behind; drop this sample rather than
		// overwrite ones it hasn't consumed yet, or stall the CPU thread
#ifdef DEBUG_BUILD
		debug_samples_dropped++;
#endif
		return;
	}

	ring_buffer[write_pos] = sample;
	atomic_store_explicit(&ring_write_pos, next, memory_order_release);
}

// consumer side: called only from the SDL audio thread (via sound_callback)
static int16_t ring_pop() {
	size_t read_pos = atomic_load_explicit(&ring_read_pos, memory_order_relaxed);

	if (read_pos == atomic_load_explicit(&ring_write_pos, memory_order_acquire)) {
		// underrun: nothing produced yet. Jumping straight to 0 is itself a
		// sharp discontinuity (broadband, "unfiltered"-sounding click) on
		// top of the missing content, so fade the last real sample toward
		// silence instead - within ~2ms this settles to true silence, but a
		// brief stall doesn't slam the output to a hard edge.
		ring_last_sample = (int16_t) (ring_last_sample * 0.95f);
#ifdef DEBUG_BUILD
		atomic_fetch_add_explicit(&debug_underrun_samples, 1, memory_order_relaxed);
#endif
		return ring_last_sample;
	}

	int16_t sample = ring_buffer[read_pos];
	atomic_store_explicit(&ring_read_pos, (read_pos + 1) % RING_BUFFER_SIZE, memory_order_release);
	ring_last_sample = sample;
#ifdef DEBUG_BUILD
	atomic_fetch_add_explicit(&debug_samples_consumed, 1, memory_order_relaxed);
#endif
	return sample;
}

void sound_step (int cycles) {
	frame_seq_cycles += cycles;

	while (frame_seq_cycles >= FRAME_SEQUENCER_PERIOD) {
		frame_seq_cycles -= FRAME_SEQUENCER_PERIOD;
		frame_sequencer_tick();
	}

	sample_cycle_acc += cycles;

	while (sample_cycle_acc >= CYCLES_PER_SAMPLE) {
		sample_cycle_acc -= CYCLES_PER_SAMPLE;
#ifdef DEBUG_BUILD
		debug_samples_generated++;
#endif
		ring_push(generate_sample());
	}
}

void sound_callback(void* userdata, uint8_t* stream, int len) {
	int16_t* buffer = (int16_t*) stream;
	len /= sizeof(*buffer);

	for (int i = 0; i < len; ++i) {
		buffer[i] = ring_pop();
	}
}

#ifdef DEBUG_BUILD
// one compact line per video frame: how much real (wall-clock) time the
// frame took, how the CPU thread spent that frame's cycles, and what the
// audio pipeline did with the samples produced during it. Meant to be
// collected by redirecting stdout to a file for a few seconds around a
// suspect moment (e.g. `./smallconsole > log.txt`), not left running -
// at 60 lines/sec this is roughly 6-10KB/sec of text.
void sound_log_frame_debug (uint32_t frame_number, float frame_wall_ms, uint32_t cpu_halted_cycles, uint32_t cpu_active_cycles) {
	uint32_t consumed = atomic_exchange_explicit(&debug_samples_consumed, 0, memory_order_relaxed);
	uint32_t underrun = atomic_exchange_explicit(&debug_underrun_samples, 0, memory_order_relaxed);

	size_t write_pos = atomic_load_explicit(&ring_write_pos, memory_order_relaxed);
	size_t read_pos = atomic_load_explicit(&ring_read_pos, memory_order_relaxed);
	size_t fill = (write_pos + RING_BUFFER_SIZE - read_pos) % RING_BUFFER_SIZE;

	println(
		"AUDIODBG frame=%u wall_ms=%.2f halt_cyc=%u act_cyc=%u gen=%u drop=%u cons=%u under=%u fill=%u trig1=%u trig2=%u trig3=%u trig4=%u nr52=%02x en=%d%d%d%d",
		frame_number, frame_wall_ms, cpu_halted_cycles, cpu_active_cycles,
		debug_samples_generated, debug_samples_dropped, consumed, underrun, (unsigned) fill,
		debug_triggers[0], debug_triggers[1], debug_triggers[2], debug_triggers[3],
		sound_read_reg(0xFF26),
		state.channel1.enabled, state.channel2.enabled, state.channel3.enabled, state.channel4.enabled
	);

	debug_samples_generated = 0;
	debug_samples_dropped = 0;
	debug_triggers[0] = 0;
	debug_triggers[1] = 0;
	debug_triggers[2] = 0;
	debug_triggers[3] = 0;
}
#endif
