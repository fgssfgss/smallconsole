#include "common.h"
#include <stdarg.h>
#include <math.h>
#include "joypad.h"
#include "rom.h"

static FILE              *log_file        = NULL;
static SDL_Window        *window          = NULL;
static SDL_Renderer      *renderer        = NULL;
static key_handler       key_up_handler   = NULL;
static key_handler       key_down_handler = NULL;
static SDL_AudioDeviceID audio_dev        = 0;

static void audio_callback(void* userdata, uint8_t* stream, int len);
static sound_callback_fn callback_fn;

void common_init (void) {
	log_file = stdout;

	SDL_Init(SDL_INIT_EVERYTHING);

	window = SDL_CreateWindow(
		"smallconsole", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, RENDER_WIDTH, RENDER_HEIGHT, 0
	);

	// software renderer works much faster
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
	SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
	SDL_RenderSetScale(renderer, (float) RENDER_SCALE, (float) RENDER_SCALE);

	// sound init
	SDL_AudioSpec desiredSpec, givenSpec;

	desiredSpec.freq     = SOUND_SAMPLE_RATE;
	desiredSpec.format   = AUDIO_S16SYS;
	desiredSpec.channels = 1;
	desiredSpec.samples  = 1024;
	desiredSpec.callback = audio_callback;
	desiredSpec.userdata = NULL;

	audio_dev = SDL_OpenAudioDevice(NULL, 0, &desiredSpec, &givenSpec, 0);
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

	size_t read_res = fread(romdata, 0x1, romsize, rom);
	if (read_res != romsize) {
		println("Wtf? Can't read full file");
		return;
	}
	fclose(rom);

	if (romdata[0x0147] != 0x00 && romdata[0x0147] != 0x01 && romdata[0x0147] != 0x02 && romdata[0x0147] != 0x03) {
		println("Mapper is not supported, mapper version is %02x", romdata[0x0147]);
		return;
	}
	else {
		println("Actual filesize is 0x%x", romsize);
		println("Mapper is %02x", romdata[0x0147]);
		println("ROM size is %02x", romdata[0x0148]);
		println("RAM size is %02x", romdata[0x0149]);
	}

	rom_load(romdata, romsize, romdata[0x0147]);
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

void sound_set_callback(sound_callback_fn callback) {
	callback_fn = callback;

	SDL_PauseAudioDevice(audio_dev, 0);
}

static void audio_callback(void* userdata, uint8_t* stream, int len) {
	if (callback_fn != NULL) {
		callback_fn(userdata, stream, len);
	}
}

void keyboard_set_handlers (key_handler key_down, key_handler key_up) {
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
	case SDLK_z:
		key = JOYPAD_BUTTON_A;
		break;
	case SDLK_x:
		key = JOYPAD_BUTTON_B;
		break;
	case SDLK_RETURN:
		key = JOYPAD_BUTTON_START;
		break;
	case SDLK_SPACE:
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

void common_shutdown (void) {
	SDL_PauseAudioDevice(audio_dev, 1);
	SDL_CloseAudioDevice(audio_dev);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

// just a log routine
void println (const char *message, ...) {
	va_list arg;

	va_start(arg, message);
	vfprintf(log_file, message, arg);
	va_end(arg);

	fprintf(log_file, "\r\n");
	fflush(log_file);
}

void printl (const char *message, ...) {
	va_list arg;

	va_start(arg, message);
	vfprintf(log_file, message, arg);
	va_end(arg);

	fflush(log_file);
}
