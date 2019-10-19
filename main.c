#include "common.h"
#include "cpu.h"

int main() {
	init_common();

	printl("EMULATOR INIT");
	// dump this file from somewhere
    init_cpu();
    cpu_run();
    //cpu_load_rom("tetris.gb");

	shutdown_common();
	return 0;
}