#ifndef _SOUND_H_
#define _SOUND_H_

#include "common.h"

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


void sound_init (void);

void sound_write_reg (uint16_t addr, uint8_t val);

uint8_t sound_read_reg (uint16_t addr);

void sound_write_wavetable (uint16_t addr, uint8_t val);

uint8_t sound_read_wavetable (uint16_t addr);

void sound_step (int cycles);

void sound_callback(void* userdata, uint8_t* stream, int len);

#endif
