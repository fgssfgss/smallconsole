#include "common.h"
#include "cpu.h"
#include "gpu.h"
#include "joypad.h"

int main(int argc, char *argv[]) {
	SDL_Event e;
	bool      quit = false;

	init_common();

	println("EMULATOR INIT");

	file_load_rom("tetris.gb");

	keyboard_set_handlers(joypad_key_down, joypad_key_up);
	gpu_init();
	init_cpu();

	while (!quit) {
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) {
				quit = true;
			}

			if (e.type == SDL_KEYUP || e.type == SDL_KEYDOWN) {
				keyboard_handle_input(&e);
			}
		}

		cpu_tick();
	}

	shutdown_common();
	return 0;
}