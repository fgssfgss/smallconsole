#include "common.h"
#include "cpu.h"

static void cpu_step();

static uint8_t cpu_read_register(uint16_t addr);

static void cpu_write_register(uint16_t addr, uint8_t val);

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
static uint8_t rom0[0x8000]; // 0x0000 -> 0x3FFF ROM bank #0
// 0x4000 -> 0x7FFF ROM bank #1, just for no mapper roms setup
static uint8_t vram[0x4000]; // 0x8000 -> 0x9FFF Video RAM
static uint8_t sram[0x4000]; // 0xA000 -> 0xBFFF switchable RAM
static uint8_t iram[0x4000]; // 0xC000 -> 0xF0FF internal RAM
// 0xFF00 -> 0xFFFF - registers, handled via two functions

#define READBYTE(addr) ({                                                   \
                       uint8_t val = 0;                                     \
                       if (addr >= 0 && addr <= 0x7FFF) {                   \
                           val = rom0[addr];                                \
                       } else if (addr >= 0x8000 && addr <= 0x9FFF) {       \
                           val = vram[addr % 0x8000];                       \
                       } else if (addr >= 0xA000 && addr <= 0xBFFF) {       \
                           val = sram[addr % 0xA000];                       \
                       } else if (addr >= 0xC000 && addr <= 0xF0FF) {       \
                           val = iram[addr % 0xC000];                       \
                       } else {                                             \
                           val = cpu_read_register(addr);                   \
                       }                                                    \
                       val;                                                 \
})

#define WRITEBYTE(addr, val) ({                                             \
                       if (addr >= 0 && addr <= 0x7FFF) {                   \
                           rom0[addr] = val;                                \
                       } else if (addr >= 0x8000 && addr <= 0x9FFF) {       \
                           vram[addr % 0x8000] = val;                       \
                       } else if (addr >= 0xA000 && addr <= 0xBFFF) {       \
                           sram[addr % 0xA000] = val;                       \
                       } else if (addr >= 0xC000 && addr <= 0xF0FF) {       \
                           iram[addr % 0xC000]= val;                        \
                       } else {                                             \
                           cpu_write_register(addr, val);                   \
                       }                                                    \
})

#define READWORD(addr) ({                                                   \
                       uint16_t val = 0;                                    \
                       uint8_t lo = READBYTE(addr);                         \
                       uint8_t hi = READBYTE(addr + 1);                     \
                       val = (hi << 8) | lo;                                \
                       val;                                                 \
})

#define WRITEWORD(addr, val) ({                                             \
                       uint8_t lo = val & 0xFF;                             \
                       uint8_t hi = (val >> 8) & 0xFF;                      \
                       WRITEBYTE(addr, lo);                                 \
                       WRITEBYTE(addr + 1, hi);                             \
})


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

    for (int i = 0xC500; i <= 0xFFFF; ++i) {
        WRITEBYTE(i, 42);
    }

    // rom dump
    for (int i = 0, c = 0; i <= 0xFFFF; ++i) {
        printf("%02x ", READBYTE(i));

        if (++c == 16) {
            c = 0;
            printf("\r\n");
        }
    }

    cpu.pc = 0x0000;
    cpu.sp = 0x0000;
}

void cpu_run() {
    cpu_step();
}

static void cpu_step() {
    uint8_t instruction = READBYTE(cpu.pc++);
    switch (instruction) {
        case 0x00: // nop
            break;
        default:
            break;
    }
}

static uint8_t cpu_read_register(uint16_t addr) {
    return 0;
}

static void cpu_write_register(uint16_t addr, uint8_t val) {

}