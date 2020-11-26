#include "common.h"
#include "cpu.h"
#include "gpu.h"
#include "joypad.h"

int main(int argc, char *argv[]) {
	SDL_Event e    = {0};
	bool      quit = false;

	common_init();

	println("EMULATOR INIT");

	file_load_rom("11.gb");

	keyboard_set_handlers(joypad_key_down, joypad_key_up);
	gpu_init();
	cpu_init();

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

	common_shutdown();
	return 0;
}