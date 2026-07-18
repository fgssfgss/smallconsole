#include "common.h"
#include "cpu.h"
#include "gpu.h"
#include "joypad.h"
#include "timer.h"
#include "sound.h"

#ifdef DEBUG_BUILD
// per-subsystem wall-clock timing, to find out which of cpu_step/gpu_step/
// timer_step/sound_step is actually eating a slow frame's time, rather than
// inferring it from correlation. Reset and logged once per frame in main().
static Uint64 profile_cpu_ticks   = 0;
static Uint64 profile_gpu_ticks   = 0;
static Uint64 profile_timer_ticks = 0;
static Uint64 profile_sound_ticks = 0;
#endif

void render_frame () {
  int cycles = 0;
	int frame_cycles = CPU_FREQ / 60.0;
	while(frame_cycles > 0) {
#ifdef DEBUG_BUILD
		Uint64 t0 = SDL_GetPerformanceCounter();
		cycles = cpu_step();
		Uint64 t1 = SDL_GetPerformanceCounter();
		gpu_step(cycles);
		Uint64 t2 = SDL_GetPerformanceCounter();
		timer_step(cycles);
		Uint64 t3 = SDL_GetPerformanceCounter();
		sound_step(cycles);
		Uint64 t4 = SDL_GetPerformanceCounter();

		profile_cpu_ticks   += (t1 - t0);
		profile_gpu_ticks   += (t2 - t1);
		profile_timer_ticks += (t3 - t2);
		profile_sound_ticks += (t4 - t3);
#else
		cycles = cpu_step();
		gpu_step(cycles);
		timer_step(cycles);
		sound_step(cycles);
#endif
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
	sound_init();

	sound_set_callback(sound_callback);

#ifndef __EMSCRIPTEN__
	if (argc != 2) {
		println("USAGE: %s game.gb", argv[0]);
		quit = true;
	} else {
		file_load_rom(argv[1]);
	}

#ifdef DEBUG_BUILD
	uint32_t frame_number = 0;
#endif

	while (!quit || SDL_GetAudioStatus() == SDL_AUDIO_PLAYING) {
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
		uint32_t frame_wall_ms = SDL_GetTicks() - ticks;

#ifdef DEBUG_BUILD
		double perf_freq_ms = SDL_GetPerformanceFrequency() / 1000.0;
		println(
			"PROFILE frame=%u cpu_ms=%.2f gpu_ms=%.2f timer_ms=%.2f sound_ms=%.2f",
			frame_number,
			profile_cpu_ticks / perf_freq_ms,
			profile_gpu_ticks / perf_freq_ms,
			profile_timer_ticks / perf_freq_ms,
			profile_sound_ticks / perf_freq_ms
		);
		profile_cpu_ticks = 0;
		profile_gpu_ticks = 0;
		profile_timer_ticks = 0;
		profile_sound_ticks = 0;

		uint32_t halted_cycles = 0, active_cycles = 0;
		cpu_get_debug_stats(&halted_cycles, &active_cycles);
		sound_log_frame_debug(frame_number++, (float) frame_wall_ms, halted_cycles, active_cycles);
#endif

		int32_t time_to_delay = 1000/60.0f - frame_wall_ms;

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
