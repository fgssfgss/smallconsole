#ifndef _JOYPAD_H_
#define _JOYPAD_H_

#include "common.h"

enum {
	JOYPAD_LEFT = 0,
	JOYPAD_RIGHT,
	JOYPAD_UP,
	JOYPAD_DOWN,
	JOYPAD_BUTTON_A,
	JOYPAD_BUTTON_B,
	JOYPAD_BUTTON_SELECT,
	JOYPAD_BUTTON_START
};

void joypad_key_up (int key);

void joypad_key_down (int key);

uint8_t joypad_read_reg (void);

void joypad_write_reg (uint8_t val);

#endif /* _JOYPAD_H_ */