#include "common.h"
#include "cpu.h"

int main() {
	init_common();

	printl("EMULATOR INIT");
	// dump this file from somewhere
	init_cpu("tetris.gb");
    cpu_run();

	shutdown_common();
	return 0;
}