#include "common.h"
#include "cpu.h"
#include "gpu.h"

int main(int argc, char *argv[]) {
	init_common();

	println("EMULATOR INIT");

	gpu_init();
	init_cpu();
	cpu_load_rom("tetris.gb");

	cpu_run();

	shutdown_common();
	return 0;
}