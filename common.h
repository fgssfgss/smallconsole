#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

/* switch to enable GPU debug window and debug output*/
#define DEBUG_BUILD

#define ALWAYS_INLINE __attribute__((always_inline))

#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 144

#define RENDER_SCALE 2
#define RENDER_WIDTH (SCREEN_WIDTH * RENDER_SCALE)
#define RENDER_HEIGHT (SCREEN_HEIGHT * RENDER_SCALE)

typedef struct rom_mapper_func {
	void (*init)(uint8_t *rom, uint64_t filesize);
	uint8_t (*read)(uint16_t address);
	void (*write)(uint16_t address, uint8_t val);
} rom_mapper_func_t;

void common_init ();

void common_shutdown ();

void println (const char *message, ...);

void printl (const char *message, ...);

void file_load_rom (const char *rom_filename);

void screen_clear (void);

void screen_vsync (void);

void screen_put_pixel (int x, int y, uint8_t r, uint8_t g, uint8_t b);

void keyboard_set_handlers (void (*key_down) (int key), void (*key_up) (int key));

void keyboard_handle_input (SDL_Event *event);

#endif /* _COMMON_H_ */
