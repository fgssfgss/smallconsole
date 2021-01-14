#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <SDL2/SDL.h>

/* switch to enable GPU debug window and debug output*/
#undef DEBUG_BUILD

#define ALWAYS_INLINE __attribute__((always_inline))

#define SCREEN_WIDTH  160
#define SCREEN_HEIGHT 144

#define RENDER_SCALE 2
#define RENDER_WIDTH (SCREEN_WIDTH * RENDER_SCALE)
#define RENDER_HEIGHT (SCREEN_HEIGHT * RENDER_SCALE)

#define SOUND_FREQUENCY 44100
#define SOUND_SAMPLE_RATE 1024
#define CPU_FREQ 4213440 // in HZ

typedef void (*key_handler) (int key);

void common_init ();

void common_shutdown ();

void println (const char *message, ...);

void printl (const char *message, ...);

void file_load_rom (const char *rom_filename);

void screen_clear (void);

void screen_vsync (void);

void screen_put_pixel (int x, int y, uint8_t r, uint8_t g, uint8_t b);

void audio_send_samples (int16_t* samples, int len);

void keyboard_set_handlers (key_handler key_down, key_handler key_up);

void keyboard_handle_input (SDL_Event *event);

#endif /* _COMMON_H_ */
