#include "common.h"
#include "cpu.h"

int main() {
	init_common();

    println("EMULATOR INIT");
	// dump this file from somewhere
    init_cpu();
    cpu_load_rom("tetris.gb");
    cpu_run();

	shutdown_common();
	return 0;
}