#include "common.h"
#include "cpu.h"
#include "gpu.h"

int main(int argc, char *argv[]) {
	SDL_Event e;
	bool      quit = false;

	init_common();

	println("EMULATOR INIT");

	gpu_init();
	init_cpu();
	cpu_load_rom("tetris.gb");

	while (!quit) {
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) {
				quit = true;
			}
		}

		cpu_tick();
	}

	shutdown_common();
	return 0;
}