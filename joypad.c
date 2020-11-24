#include "joypad.h"

// TODO: interrupts!

enum {
	INPUT_RIGHT_OR_A            = 0x1,
	INPUT_LEFT_OR_B             = 0x2,
	INPUT_UP_OR_SELECT          = 0x4,
	INPUT_DOWN_OR_START         = 0x8,
	INPUT_SELECT_DIRECTION_KEYS = 0x10,
	INPUT_SELECT_BUTTON_KEYS    = 0x20
};

static uint8_t reg      = 0xFF;
static int     state[8] = {0};

void joypad_key_up (int key) {
	state[key] = 0;
}

void joypad_key_down (int key) {
	state[key] = 1;
}

// TODO CHECK THIS LOGIC, COULD BE BROKEN DUE TO ISSUE WITH SOME CPU COMMAND
uint8_t joypad_read_reg (void) {
	if (reg & INPUT_SELECT_DIRECTION_KEYS) {
		if (state[JOYPAD_UP]) {
			reg &= ~INPUT_UP_OR_SELECT;
		}

		if (state[JOYPAD_DOWN]) {
			reg &= ~INPUT_DOWN_OR_START;
		}

		if (state[JOYPAD_LEFT]) {
			reg &= ~INPUT_LEFT_OR_B;
		}

		if (state[JOYPAD_RIGHT]) {
			reg &= ~INPUT_RIGHT_OR_A;
		}
	}
	else if (reg & INPUT_SELECT_BUTTON_KEYS) {
		if (state[JOYPAD_BUTTON_A]) {
			reg &= ~INPUT_RIGHT_OR_A;
		}

		if (state[JOYPAD_BUTTON_B]) {
			reg &= ~INPUT_LEFT_OR_B;
		}

		if (state[JOYPAD_BUTTON_SELECT]) {
			reg &= ~INPUT_UP_OR_SELECT;
		}

		if (state[JOYPAD_BUTTON_START]) {
			reg &= ~INPUT_DOWN_OR_START;
		}
	}

	return reg;
}

void joypad_write_reg (uint8_t val) {
	reg = 0xff;
	reg &= ~val;
}
