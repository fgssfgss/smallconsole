#include "common.h"
#include <stdarg.h>
#include "joypad.h"
#include "rom.h"

typedef void (*key_handler) (int key);

static FILE         *log             = NULL;
static SDL_Window   *window          = NULL;
static SDL_Renderer *renderer        = NULL;
static key_handler  key_up_handler   = NULL;
static key_handler  key_down_handler = NULL;

void init_common (void) {
	log = stdout;

	SDL_Init(SDL_INIT_VIDEO);

	window = SDL_CreateWindow(
		"smallconsole", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0
	);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
}

void file_load_rom (const char *rom_filename) {
	FILE *rom    = fopen(rom_filename, "rb");
	long romsize = 0;
	if (rom == NULL) {
		println("Failed to open rom file \'%s\'", rom_filename);
		return;
	}

	fseek(rom, 0, SEEK_END);
	romsize = ftell(rom);
	fseek(rom, 0, SEEK_SET);  /* same as rewind(f); */

	uint8_t *romdata = (void *) malloc(romsize*sizeof(uint8_t));

	fread(romdata, 0x1, romsize, rom);
	fclose(rom);

	if (romdata[0x0147] != 0x00) {
		println("Mapper is not supported");
		return;
	}
	else {
		println("Mapper is %02x", romdata[0x0147]);
		println("ROM size is %02x", romdata[0x0148]);
		println("RAM size is %02x", romdata[0x0149]);
	}

	rom_load(romdata);
}

void screen_put_pixel (int x, int y, uint8_t r, uint8_t g, uint8_t b) {
	SDL_SetRenderDrawColor(renderer, r, g, b, 255);
	SDL_RenderDrawPoint(renderer, x, y);
}

void screen_vsync (void) {
	SDL_RenderPresent(renderer);
}

void screen_clear (void) {
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer);
}

void keyboard_set_handlers (void (*key_down) (int key), void (*key_up) (int key)) {
	key_up_handler   = key_up;
	key_down_handler = key_down;
}

void keyboard_handle_input (SDL_Event *event) {
	int key = 0;
	switch (event->key.keysym.sym) {
	case SDLK_LEFT:
		key = JOYPAD_LEFT;
		break;
	case SDLK_RIGHT:
		key = JOYPAD_RIGHT;
		break;
	case SDLK_UP:
		key = JOYPAD_UP;
		break;
	case SDLK_DOWN:
		key = JOYPAD_DOWN;
		break;
	case SDLK_a:
		key = JOYPAD_BUTTON_A;
		break;
	case SDLK_s:
		key = JOYPAD_BUTTON_B;
		break;
	case SDLK_SPACE:
		key = JOYPAD_BUTTON_START;
		break;
	case SDLK_TAB:
		key = JOYPAD_BUTTON_SELECT;
		break;
	default:
		return;
	}

	if (event->type == SDL_KEYUP) {
		key_up_handler(key);
	}
	else if (event->type == SDL_KEYDOWN) {
		key_down_handler(key);
	}
}

void shutdown_common (void) {
	SDL_DestroyWindow(window);
	SDL_Quit();
}

// just a log routine
void println (const char *message, ...) {
	va_list arg;

	va_start(arg, message);
	vfprintf(log, message, arg);
	va_end(arg);

	fprintf(log, "\r\n");
	fflush(log);
}

void printl(const char *message, ...) {
	va_list arg;

	va_start(arg, message);
	vfprintf(log, message, arg);
	va_end(arg);

	fflush(log);
}