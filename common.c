#include "common.h"
#include <stdarg.h>

static FILE *log = NULL;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

void init_common(void) {
	log = stdout;

	SDL_Init(SDL_INIT_VIDEO);

	window = SDL_CreateWindow(
		"smallconsole",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		160,
		144,
		0
	);

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
}

void screen_put_pixel(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
	SDL_SetRenderDrawColor(renderer, r, g, b, 255);
	SDL_RenderDrawPoint(renderer, x, y);
}

void screen_vsync(void) {
	SDL_RenderPresent(renderer);
}

void screen_clear(void) {
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(renderer);
}

void shutdown_common(void) {
	SDL_DestroyWindow(window);
	SDL_Quit();
}

// just a log routine
void println(const char *message, ...) {
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