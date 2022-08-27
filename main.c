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

int main(int argc, char *argv[]) {
	SDL_Event e     = {0};
	bool      quit  = false;
	int32_t   ticks = 0;

	common_init();

	println("EMULATOR INIT");

	file_load_rom("zelda.gb");

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

		ticks = SDL_GetTicks();
		render_frame();
		int32_t time_to_delay = 1000/60.0f - (SDL_GetTicks() - ticks);

		if (time_to_delay > 0) {
			SDL_Delay(time_to_delay);
		}
	}

	common_shutdown();
	return 0;
}
