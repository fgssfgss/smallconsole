#include "common.h"
#include "cpu.h"

typedef struct __cpu_state {
    struct {
	union {
	    struct {
		uint8_t a;
		uint8_t f;
	    };
	    uint16_t af;
	};
    };
    struct {
	union {
	    struct {
		uint8_t b;
		uint8_t c;
	    };
	    uint16_t bc;
	};
    };
    struct {
	union {
	    struct {
		uint8_t d;
		uint8_t e;
	    };
	    uint16_t de;
	};
    };
    struct {
	union {
	    struct {
		uint8_t h;
		uint8_t l;
	    };
	    uint16_t hl;
	};
    };
    uint16_t sp;
    uint16_t pc;

    uint8_t flag;
} cpu_state;

// CPU STATE
static cpu_state cpu;

// ALL KINDS OF MEMORY
static uint8_t rom0[0x8000]; // 0x0000 -> 0x3FFF ROM bank #0 and 0x4000 -> 0x7FFF ROM bank #1, just for no mapper roms setup
static uint8_t vram[0x4000]; // 0x8000 -> 0x9FFF Video RAM
static uint8_t sram[0x4000]; // 0xA000 -> 0xBFFF switchable RAM
static uint8_t iram[0x4000]; // 0xC000 -> 0xF0FF internal RAM

void init_cpu(const char *rom_filename) {
	FILE *rom = fopen(rom_filename, "rb");
	long romsize = 0;
	if (rom == NULL) {
		printl("Failed to open rom file \'%s\'", rom_filename);
		return;
	}

	fseek(rom, 0, SEEK_END);
	romsize = ftell(rom);
	fseek(rom, 0, SEEK_SET);  /* same as rewind(f); */

	if (romsize != 0x8000) {
		printl("File size is not supported");
		return;
	}

	fread(rom0, 0x1, 0x8000, rom);
	fclose(rom);

	// rom dump
	for (int i = 0, c = 0; i < 0x8000; ++i) {
		printf("%02x ", rom0[i]);

		if (++c == 16) {
			c = 0;
			printf("\r\n");
		}
	}
}

void run() {
	// read instruction by instruction and execute them
}