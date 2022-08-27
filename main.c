#include "common.h"
#include "cpu.h"
#include "gpu.h"
#include "joypad.h"
#include "timer.h"

void render_frame () {
    int cycles = 0;
	int frame_cycles = 71025;
	while(frame_cycles > 0) {
		cycles = cpu_step();
		gpu_step(cycles);
		timer_step(cycles);

		frame_cycles -= cycles;
	}
}

#ifdef __EMSCRIPTEN__
static EM_BOOL key_callback(int event_type, const EmscriptenKeyboardEvent *event, void *user_data) {
	SDL_Event e = {0};

	switch (event_type) {
		case EMSCRIPTEN_EVENT_KEYDOWN:
			e.type = SDL_KEYDOWN;
			break;
		case EMSCRIPTEN_EVENT_KEYUP:
			e.type = SDL_KEYUP;
			break;
		default:
			return EM_TRUE;
	}

	switch (event->keyCode) {
		case 90:
			e.key.keysym.sym = SDLK_z;
			break;
		case 65:
			e.key.keysym.sym = SDLK_x;
			break;
		case 32:
			e.key.keysym.sym = SDLK_SPACE;
			break;
		case 13:
			e.key.keysym.sym = SDLK_RETURN;
			break;
		case 38:
			e.key.keysym.sym = SDLK_UP;
			break;
		case 40:
			e.key.keysym.sym = SDLK_DOWN;
			break;
		case 37:
			e.key.keysym.sym = SDLK_LEFT;
			break;
		case 39:
			e.key.keysym.sym = SDLK_RIGHT;
			break;
		default:
			return EM_TRUE;
	}

	keyboard_handle_input(&e);

	return EM_TRUE;
}
#endif

int main(int argc, char *argv[]) {
	SDL_Event e     = {0};
	bool      quit  = false;
	int32_t   ticks = 0;

	common_init();

	println("EMULATOR INIT");

	keyboard_set_handlers(joypad_key_down, joypad_key_up);
	gpu_init();
	cpu_init();

#ifndef __EMSCRIPTEN__
	file_load_rom("zelda.gb");

	while (!quit) {
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) {
				quit = true;
			}

			if (e.type == SDL_KEYUP || e.type == SDL_KEYDOWN) {
				keyboard_handle_input(&e);
			}
		}

		ticks = SDL_GetTicks();
		render_frame();
		int32_t time_to_delay = 1000/60.0f - (SDL_GetTicks() - ticks);

		if (time_to_delay > 0) {
			SDL_Delay(time_to_delay);
		}
	}
#else
	file_load_rom("game.gb");
	emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, key_callback);
	emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, true, key_callback);
	emscripten_set_main_loop(render_frame, 60, 1);
#endif /* __EMSCRIPTEN__ */

	common_shutdown();
	return 0;
}
