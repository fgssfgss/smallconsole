#include "common.h"
#include "cpu.h"

enum flags {
    C = 4,
    H = 5,
    N = 6,
    Z = 7
};

enum registers {
    REG_B = 0,
    REG_C,
    REG_D,
    REG_E,
    REG_H,
    REG_L,
    REG_HL,
    REG_A
};

static enum registers map_register(uint8_t opcode) {
    enum registers reg_code = (opcode & 0xF) % 8;
    return reg_code;
}

// I assume that cpu_state is `cpu` object
#define GET_FLAG(flag)   ((cpu.f >> (flag)) & 0x1)

#define SET_FLAGS(zflag, nflag, hflag, cflag) \
                    ((!!(zflag) << Z) | (!!(nflag) << N) | (!!(hflag) << H) | (!!(cflag) << C) | 0)

// TODO: fixme: H flag set
#define SET_INC_FLAGS(data) (cpu.f = SET_FLAGS(data == 0, 0, (((data & 0xF) + ((data-1) & 0xF)) > 0xF), GET_FLAG(C)))
#define SET_DEC_FLAGS(data) (cpu.f = SET_FLAGS(data == 0, 1, (((data & 0xF) + ((data+1) & 0xF)) > 0xF), GET_FLAG(C)))
#define SET_AND_FLAGS() (cpu.f = SET_FLAGS(cpu.a == 0, 0, 1, 0))
#define SET_xOR_FLAGS() (cpu.f = SET_FLAGS(cpu.a == 0, 0, 0, 0))

static void cpu_dump_state();

static void cpu_step();

static uint8_t cpu_read_register(uint16_t addr);

static void cpu_write_register(uint16_t addr, uint8_t val);

static inline ALWAYS_INLINE uint8_t read_byte(uint16_t addr);

static inline ALWAYS_INLINE void write_byte(uint16_t addr, uint8_t val);

static inline ALWAYS_INLINE uint16_t read_word(uint16_t addr);

static inline ALWAYS_INLINE void write_word(uint16_t addr, uint16_t val);

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

    bool boot_rom_enabled;
} cpu_state;

// this is a table for cycles count for each instruction
// we might modify cycles counter in cpu_step()
static int cycles_main_opcodes[256] = {
        4,  12, 8,  8,  4,  4,  8,  4,  20, 8,  8,  8,  4,  4,  8,  4,
        4,  12, 8,  8,  4,  4,  8,  4,  12, 8,  8,  8,  4,  4,  8,  4,
        8,  12, 8,  8,  4,  4,  8,  4,  8,  8,  8,  8,  4,  4,  8,  4,
        8,  12, 8,  8,  12, 12, 12, 4,  8,  8,  8,  8,  4,  4,  8,  4,
        4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
        4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
        4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
        8,  8,  8,  8,  8,  8,  4,  8,  4,  4,  4,  4,  4,  4,  8,  4,
        4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
        4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
        4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
        4,  4,  4,  4,  4,  4,  8,  4,  4,  4,  4,  4,  4,  4,  8,  4,
        8,  12, 12, 16, 12, 16, 8,  16, 8,  16, 12, 4,  12, 24, 8,  16,
        8,  12, 12, 0,  12, 16, 8,  16, 8,  16, 12, 0,  12, 0,  8,  16,
        12, 12, 8,  0,  0,  16, 8,  16, 16, 4,  16, 0,  0,  0,  8,  16,
        12, 12, 8,  4,  0,  16, 8,  16, 12, 8,  16, 4,  0,  0,  8,  16
};
static int cycles_0xCB_opcodes[256] = {
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8,
        8,  8,  8,  8,  8,  8,  16, 8,  8,  8,  8,  8,  8,  8,  16, 8
};

// CPU STATE
static cpu_state cpu;

// ALL KINDS OF MEMORY
static uint8_t rom0[0x8000]; // rom banks 0..1
static uint8_t vram[0x4000]; // video ram, maybe we need to pass it to ppu, 8 kbytes
static uint8_t sram[0x4000]; // switchable ram from cartridge
static uint8_t iram[0x4000]; // internal ram, 8kbytes
static uint8_t zeropage[0x7F]; // high mem
// 0x0000 -> 0x3FFF - ROM bank #0
// 0x4000 -> 0x7FFF - ROM bank #n, we map to 1 bank just for no mapper roms setup
// 0x8000 -> 0x9FFF - Video RAM
// 0xA000 -> 0xBFFF - switchable RAM
// 0xC000 -> 0xDFFF - internal RAM
// 0xE000 -> 0xFDFF - mirroring of 0xC000 region
// 0xFE00 -> 0xFE9F - OAM
// 0xFEA0 -> 0xFEFF - unusable memory area
// 0xFF00 -> 0xFF7F - registers, handled via two functions
// 0xFF80 -> 0xFFFE - high RAM or zeropage
// 0xFFFF -> 0xFFFF - interrupt register

static uint8_t boot_rom[256] = {
        0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32, 0xcb, 0x7c, 0x20, 0xfb,
        0x21, 0x26, 0xff, 0x0e, 0x11, 0x3e, 0x80, 0x32, 0xe2, 0x0c, 0x3e, 0xf3,
        0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0, 0x47, 0x11, 0x04, 0x01,
        0x21, 0x10, 0x80, 0x1a, 0xcd, 0x95, 0x00, 0xcd, 0x96, 0x00, 0x13, 0x7b,
        0xfe, 0x34, 0x20, 0xf3, 0x11, 0xd8, 0x00, 0x06, 0x08, 0x1a, 0x13, 0x22,
        0x23, 0x05, 0x20, 0xf9, 0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99,
        0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20, 0xf9, 0x2e, 0x0f, 0x18,
        0xf3, 0x67, 0x3e, 0x64, 0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x04,
        0x1e, 0x02, 0x0e, 0x0c, 0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa, 0x0d, 0x20,
        0xf7, 0x1d, 0x20, 0xf2, 0x0e, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62,
        0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x06, 0x7b, 0xe2, 0x0c, 0x3e,
        0x87, 0xe2, 0xf0, 0x42, 0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x05, 0x20,
        0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x06, 0x04, 0xc5, 0xcb, 0x11, 0x17,
        0xc1, 0xcb, 0x11, 0x17, 0x05, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9,
        0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b, 0x03, 0x73, 0x00, 0x83,
        0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
        0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63,
        0x6e, 0x0e, 0xec, 0xcc, 0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e,
        0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c, 0x21, 0x04, 0x01, 0x11,
        0xa8, 0x00, 0x1a, 0x13, 0xbe, 0x20, 0xfe, 0x23, 0x7d, 0xfe, 0x34, 0x20,
        0xf5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xfb, 0x86, 0x20, 0xfe,
        0x3e, 0x01, 0xe0, 0x50
};

static inline ALWAYS_INLINE uint8_t read_byte(uint16_t addr) {
    uint8_t val = 0;
    if (addr >= 0 && addr <= 0x7FFF) {
        val = rom0[addr];
        if (addr >= 0 && addr <= 0x00FF && cpu.boot_rom_enabled) {
            val = boot_rom[addr];
        }
    } else if (addr >= 0x8000 && addr <= 0x9FFF) {
        val = vram[addr % 0x8000];
    } else if (addr >= 0xA000 && addr <= 0xBFFF) {
        val = sram[addr % 0xA000];
    } else if (addr >= 0xC000 && addr <= 0xDFFF) {
        val = iram[addr % 0xC000];
    } else if (addr >= 0xE000 && addr <= 0xFDFF) {
        val = iram[addr % 0xE000];
    } else if (addr >= 0xFE00 && addr <= 0xFE9F) {
        // oam memory
    } else if (addr >= 0xFEA0 && addr <= 0xFEFF) {
        // unusable memory
    } else if (addr >= 0xFF00 && addr <= 0xFF7F) {
        val = cpu_read_register(addr);
    } else if (addr >= 0xFF80 && addr <= 0xFFFE) {
        val = zeropage[addr % 0xFF80];
    } else {
        // interrupt register
    }
    return val;
}

static inline ALWAYS_INLINE void write_byte(uint16_t addr, uint8_t val) {
    if (addr >= 0 && addr <= 0x7FFF) {
        rom0[addr] = val;
        if (addr >= 0 && addr <= 0x00FF && cpu.boot_rom_enabled) {
            boot_rom[addr] = val;
        }
    } else if (addr >= 0x8000 && addr <= 0x9FFF) {
        vram[addr % 0x8000] = val;
    } else if (addr >= 0xA000 && addr <= 0xBFFF) {
        sram[addr % 0xA000] = val;
    } else if (addr >= 0xC000 && addr <= 0xF0FF) {
        iram[addr % 0xC000] = val;
    } else if (addr >= 0xE000 && addr <= 0xFDFF) {
        iram[addr % 0xE000] = val;
    } else if (addr >= 0xFE00 && addr <= 0xFE9F) {
        // oam memory
    } else if (addr >= 0xFEA0 && addr <= 0xFEFF) {
        // unusable memory
    } else if (addr >= 0xFF00 && addr <= 0xFF7F) {
        cpu_write_register(addr, val);
    } else if (addr >= 0xFF80 && addr <= 0xFFFE) {
        zeropage[addr % 0xFF80] = val;
    } else {
        // interrupt register
    }
}

static inline ALWAYS_INLINE uint16_t read_word(uint16_t addr) {
    uint8_t lo = read_byte(addr);
    uint8_t hi = read_byte(addr + 1);
    return (hi << 8) | lo;
}

static inline ALWAYS_INLINE void write_word(uint16_t addr, uint16_t val) {
    uint8_t lo = val & 0xFF;
    uint8_t hi = (val >> 8) & 0xFF;
    write_byte(addr, lo);
    write_byte(addr + 1, hi);
}

void cpu_load_rom(const char *rom_filename) {
    FILE *rom = fopen(rom_filename, "rb");
    long romsize = 0;
    if (rom == NULL) {
        println("Failed to open rom file \'%s\'", rom_filename);
        return;
    }

    fseek(rom, 0, SEEK_END);
    romsize = ftell(rom);
    fseek(rom, 0, SEEK_SET);  /* same as rewind(f); */

    if (romsize != 0x8000) {
        println("File size is not supported");
        return;
    }

    fread(rom0, 0x1, 0x8000, rom);
    fclose(rom);

    // memory dump
    for (int i = 0, c = 0; i <= 0xFFFF; ++i) {
        if (c == 0) {
            printl("0x%04x:\t", i);
        }

        printl("%02x ", read_byte(i));

        if (++c == 16) {
            c = 0;
            println(" ");
        }
    }
}

void init_cpu() {
    cpu.pc = 0x0000;
    cpu.sp = 0x0000;
    cpu.boot_rom_enabled = 1;
}

void cpu_run() {
    cpu_step();
    cpu_step();
    cpu_step();
    cpu_step();
    cpu_step();
    cpu_step();
    cpu_step();
}


static void cpu_prefix_cb_handle(int *cycles) {
    uint8_t instruction = read_byte(cpu.pc++);
    *cycles += cycles_0xCB_opcodes[instruction];
    enum registers reg = map_register(instruction);
    // now we should mask register bits in instruction
    instruction &= 0xF8;
    println("Instruction is 0x%02x, cycles %d, affected reg %d", instruction, *cycles, reg);

    switch(instruction) {
        case 0x00: // RLC
            break;
        case 0x08: // RRC
            break;
        case 0x10: // RL
            break;
        case 0x18: // RR
            break;
        case 0x20: // SLA
            break;
        case 0x28: // SRA
            break;
        case 0x30: // SWAP
            break;
        case 0x38: // SRL
            break;
        case 0x40: // BIT 0
            break;
        case 0x48: // BIT 1
            break;
        case 0x50: // BIT 2
            break;
        case 0x58: // BIT 3
            break;
        case 0x60: // BIT 4
            break;
        case 0x68: // BIT 5
            break;
        case 0x70: // BIT 6
            break;
        case 0x78: // BIT 7
            break;
        case 0x80: // RES 0
            break;
        case 0x88: // RES 1
            break;
        case 0x90: // RES 2
            break;
        case 0x98: // RES 3
            break;
        case 0xA0: // RES 4
            break;
        case 0xA8: // RES 5
            break;
        case 0xB0: // RES 6
            break;
        case 0xB8: // RES 7
            break;
        case 0xC0: // SET 0
            break;
        case 0xC8: // SET 1
            break;
        case 0xD0: // SET 2
            break;
        case 0xD8: // SET 3
            break;
        case 0xE0: // SET 4
            break;
        case 0xE8: // SET 5
            break;
        case 0xF0: // SET 6
            break;
        case 0xF8: // SET 7
            break;
        default:
            break;
    }
}

// TODO: implement setting flags after steps
static void cpu_step() {
    uint8_t instruction = read_byte(cpu.pc++);
    int cycles = cycles_main_opcodes[instruction];
    println("instr 0x%02x cycles %d", instruction, cycles);
    switch (instruction) {
        case 0x00: // NOP
            break;
        case 0x01: // LD BC, d16
            cpu.bc = read_word(cpu.pc);
            cpu.pc += 2;
            break;
        case 0x02: // LD (BC), A
            write_byte(cpu.bc, cpu.a);
            break;
        case 0x03: // INC BC
            cpu.bc++;
            break;
        case 0x04: // INC B
            cpu.b++;
            SET_INC_FLAGS(cpu.b);
            break;
        case 0x05: // DEC B
            cpu.b--;
            SET_DEC_FLAGS(cpu.b);
            break;
        case 0x06: // LD B, d8
            cpu.b = read_byte(cpu.pc);
            cpu.pc++;
            break;
        case 0x07: // RLCA
            break;
        case 0x08: // LD (a16), SP
            write_word(read_word(cpu.pc), cpu.sp);
            cpu.pc += 2;
            break;
        case 0x09: // ADD HL, BC
            break;
        case 0x0A: // LD A, (BC)
            cpu.a = read_byte(cpu.bc);
            break;
        case 0x0B: // DEC BC
            cpu.bc--;
            break;
        case 0x0C: // INC C
            cpu.c++;
            SET_INC_FLAGS(cpu.c);
            break;
        case 0x0D: // DEC C
            cpu.c--;
            SET_DEC_FLAGS(cpu.c);
            break;
        case 0x0E: // LD C, d8
            cpu.c = read_byte(cpu.pc);
            cpu.pc++;
            break;
        case 0x0F: // RRCA
            break;
        case 0x10: // STOP
            break;
        case 0x11: // LD DE, d16
            cpu.de = read_word(cpu.pc);
            cpu.pc += 2;
            break;
        case 0x12: // LD (DE), A
            write_byte(cpu.de, cpu.a);
            break;
        case 0x13: // INC DE
            cpu.de++;
            break;
        case 0x14: // INC D
            cpu.d++;
            SET_INC_FLAGS(cpu.d);
            break;
        case 0x15: // DEC D
            cpu.d--;
            SET_DEC_FLAGS(cpu.d);
            break;
        case 0x16: // LD D, d8
            cpu.d = read_byte(cpu.pc);
            cpu.pc++;
            break;
        case 0x17: // RLA
            break;
        case 0x18: // JR r8
            cpu.pc += read_byte(cpu.pc);
            cpu.pc++;
            break;
        case 0x19: // ADD HL, DE
            break;
        case 0x1A: // LD A, (DE)
            cpu.a = read_byte(cpu.de);
            break;
        case 0x1B: // DEC DE
            cpu.de--;
            break;
        case 0x1C: // INC E
            cpu.e++;
            SET_INC_FLAGS(cpu.e);
            break;
        case 0x1D: // DEC E
            cpu.e--;
            SET_DEC_FLAGS(cpu.e);
            break;
        case 0x1E: // LD E, d8
            cpu.e = read_byte(cpu.pc);
            cpu.pc++;
            break;
        case 0x1F: // RRA
            break;
        case 0x20: // JR NZ, r8
            if (!GET_FLAG(Z)) {
                cpu.pc += read_byte(cpu.pc);
            }
            cpu.pc++;
            break;
        case 0x21: // LD HL, d16
            cpu.hl = read_word(cpu.pc);
            cpu.pc += 2;
            break;
        case 0x22: // LD (HL+), A
            break;
        case 0x23: // INC HL
            cpu.hl++;
            break;
        case 0x24: // INC H
            cpu.h++;
            SET_INC_FLAGS(cpu.h);
            break;
        case 0x25: // DEC H
            cpu.h--;
            SET_DEC_FLAGS(cpu.h);
            break;
        case 0x26: // LD H, d8
            cpu.h = read_byte(cpu.pc);
            cpu.pc++;
            break;
        case 0x27: { // DAA
                uint16_t reg_a = cpu.a;
                uint8_t n_flag = GET_FLAG(N);
                if ((reg_a & 0xF) > 0x9 || GET_FLAG(H)) reg_a += n_flag ? -0x6 : 0x6;
                if ((reg_a & 0xF0) > 0x90 || GET_FLAG(C)) reg_a += n_flag ? -0x60 : 0x60;
                cpu.a = reg_a & 0xFF;
                cpu.f = SET_FLAGS(reg_a == 0, n_flag, 0, (reg_a >= 0x100) | GET_FLAG(C));
                break;
            }
        case 0x28: // JR Z, r8
            if (GET_FLAG(Z)) {
                cpu.pc += read_byte(cpu.pc);
            }
            cpu.pc++;
            break;
        case 0x29: // ADD HL, HL
            break;
        case 0x2A: // LD A, (HL+)
            break;
        case 0x2B: // DEC HL
            cpu.hl--;
            break;
        case 0x2C: // INC L
            cpu.l++;
            SET_INC_FLAGS(cpu.l);
            break;
        case 0x2D: // DEC L
            cpu.l--;
            SET_DEC_FLAGS(cpu.l);
            break;
        case 0x2E: // LD L, d8
            cpu.l = read_byte(cpu.pc);
            cpu.pc++;
            break;
        case 0x2F: // CPL
            break;
        case 0x30: // JR NC, r8
            if (!GET_FLAG(C)) {
                cpu.pc += read_byte(cpu.pc);
            }
            cpu.pc++;
            break;
        case 0x31: // LD SP, d16
            cpu.sp = read_word(cpu.pc);
            cpu.pc += 2;
            break;
        case 0x32: // LD (HL-), A
            write_byte(cpu.hl--, cpu.a);
            break;
        case 0x33: // INC SP
            cpu.sp++;
            break;
        case 0x34: // INC (HL)
            // todo implement me
            break;
        case 0x35: // DEC (HL)
            // todo implement me
            break;
        case 0x36: // LD (HL), d8
            break;
        case 0x37: // SCF
            break;
        case 0x38: // JR C, r8
            if (GET_FLAG(C)) {
                cpu.pc += read_byte(cpu.pc);
            }
            cpu.pc++;
            break;
        case 0x39: // ADD HL, SP
            break;
        case 0x3A: // LD A, (HL-)
            break;
        case 0x3B: // DEC SP
            cpu.sp--;
            break;
        case 0x3C: // INC A
            cpu.a++;
            SET_INC_FLAGS(cpu.a);
            break;
        case 0x3D: // DEC A
            cpu.a--;
            SET_DEC_FLAGS(cpu.a);
            break;
        case 0x3E: // LD A, d8
            cpu.a = read_byte(cpu.pc);
            cpu.pc++;
            break;
        case 0x3F: // CCF
            break;
        case 0x40: // LD B, B
            cpu.b = cpu.b;
            break;
        case 0x41: // LD B, C
            cpu.b = cpu.c;
            break;
        case 0x42: // LD B, D
            cpu.b = cpu.d;
            break;
        case 0x43: // LD B, E
            cpu.b = cpu.e;
            break;
        case 0x44: // LD B, H
            cpu.b = cpu.h;
            break;
        case 0x45: // LD B, L
            cpu.b = cpu.l;
            break;
        case 0x46: // LD B, (HL)
            cpu.b = read_byte(cpu.hl);
            break;
        case 0x47: // LD B, A
            cpu.b = cpu.a;
            break;
        case 0x48: // LD C, B
            cpu.c = cpu.b;
            break;
        case 0x49: // LD C, C
            cpu.c = cpu.c;
            break;
        case 0x4A: // LD C, D
            cpu.c = cpu.d;
            break;
        case 0x4B: // LD C, E
            cpu.c = cpu.e;
            break;
        case 0x4C: // LD C, H
            cpu.c = cpu.h;
            break;
        case 0x4D: // LD C, L
            cpu.c = cpu.l;
            break;
        case 0x4E: // LD C, (HL)
            cpu.c = read_byte(cpu.hl);
            break;
        case 0x4F: // LD C, A
            cpu.c = cpu.a;
            break;
        case 0x50: // LD D, B
            cpu.d = cpu.b;
            break;
        case 0x51: // LD D, C
            cpu.d = cpu.c;
            break;
        case 0x52: // LD D, D
            cpu.d = cpu.d;
            break;
        case 0x53: // LD D, E
            cpu.d = cpu.e;
            break;
        case 0x54: // LD D, H
            cpu.d = cpu.h;
            break;
        case 0x55: // LD D, L
            cpu.d = cpu.l;
            break;
        case 0x56: // LD D, (HL)
            cpu.d = read_byte(cpu.hl);
            break;
        case 0x57: // LD D, A
            cpu.d = cpu.a;
            break;
        case 0x58: // LD E, B
            cpu.e = cpu.b;
            break;
        case 0x59: // LD E, C
            cpu.e = cpu.c;
            break;
        case 0x5A: // LD E, D
            cpu.e = cpu.d;
            break;
        case 0x5B: // LD E, E
            cpu.e = cpu.e;
            break;
        case 0x5C: // LD E, H
            cpu.e = cpu.h;
            break;
        case 0x5D: // LD E, L
            cpu.e = cpu.l;
            break;
        case 0x5E: // LD E, (HL)
            cpu.e = read_byte(cpu.hl);
            break;
        case 0x5F: // LD E, A
            cpu.e = cpu.a;
            break;
        case 0x60: // LD H, B
            cpu.h = cpu.b;
            break;
        case 0x61: // LD H, C
            cpu.h = cpu.c;
            break;
        case 0x62: // LD H, D
            cpu.h = cpu.d;
            break;
        case 0x63: // LD H, E
            cpu.h = cpu.e;
            break;
        case 0x64: // LD H, H
            cpu.h = cpu.h;
            break;
        case 0x65: // LD H, L
            cpu.h = cpu.l;
            break;
        case 0x66: // LD H, (HL)
            cpu.h = read_byte(cpu.hl);
            break;
        case 0x67: // LD H, A
            cpu.h = cpu.a;
            break;
        case 0x68: // LD L, B
            cpu.l = cpu.b;
            break;
        case 0x69: // LD L, C
            cpu.l = cpu.c;
            break;
        case 0x6A: // LD L, D
            cpu.l = cpu.d;
            break;
        case 0x6B: // LD L, E
            cpu.l = cpu.e;
            break;
        case 0x6C: // LD L, H
            cpu.l = cpu.h;
            break;
        case 0x6D: // LD L, L
            cpu.l = cpu.l;
            break;
        case 0x6E: // LD L, (HL)
            cpu.l = read_byte(cpu.hl);
            break;
        case 0x6F: // LD L, A
            cpu.l = cpu.a;
            break;
        case 0x70: // LD (HL), B
            write_byte(cpu.hl, cpu.b);
            break;
        case 0x71: // LD (HL), C
            write_byte(cpu.hl, cpu.c);
            break;
        case 0x72: // LD (HL), D
            write_byte(cpu.hl, cpu.d);
            break;
        case 0x73: // LD (HL), E
            write_byte(cpu.hl, cpu.e);
            break;
        case 0x74: // LD (HL), H
            write_byte(cpu.hl, cpu.h);
            break;
        case 0x75: // LD (HL), L
            write_byte(cpu.hl, cpu.l);
            break;
        case 0x76: // HALT
            break;
        case 0x77: // LD (HL), A
            write_byte(cpu.hl, cpu.a);
            break;
        case 0x78: // LD A, B
            cpu.a = cpu.b;
            break;
        case 0x79: // LD A, C
            cpu.a = cpu.c;
            break;
        case 0x7A: // LD A, D
            cpu.a = cpu.d;
            break;
        case 0x7B: // LD A, E
            cpu.a = cpu.e;
            break;
        case 0x7C: // LD A, H
            cpu.a = cpu.h;
            break;
        case 0x7D: // LD A, L
            cpu.a = cpu.l;
            break;
        case 0x7E: // LD A, (HL)
            cpu.a = read_byte(cpu.hl);
            break;
        case 0x7F: // LD A, A
            cpu.a = cpu.a;
            break;
        case 0x80: // ADD A, B
            break;
        case 0x81: // ADD A, C
            break;
        case 0x82: // ADD A, D
            break;
        case 0x83: // ADD A, E
            break;
        case 0x84: // ADD A, H
            break;
        case 0x85: // ADD A, L
            break;
        case 0x86: // ADD A, (HL)
            break;
        case 0x87: // ADD A, A
            break;
        case 0x88: // ADC A, B
            break;
        case 0x89: // ADC A, C
            break;
        case 0x8A: // ADC A, D
            break;
        case 0x8B: // ADC A, E
            break;
        case 0x8C: // ADC A, H
            break;
        case 0x8D: // ADC A, L
            break;
        case 0x8E: // ADC A, (HL)
            break;
        case 0x8F: // ADC A, A
            break;
        case 0x90: // SUB B
            break;
        case 0x91: // SUB C
            break;
        case 0x92: // SUB D
            break;
        case 0x93: // SUB E
            break;
        case 0x94: // SUB H
            break;
        case 0x95: // SUB L
            break;
        case 0x96: // SUB (HL)
            break;
        case 0x97: // SUB A
            break;
        case 0x98: // SBC A, B
            break;
        case 0x99: // SBC A, C
            break;
        case 0x9A: // SBC A, D
            break;
        case 0x9B: // SBC A, E
            break;
        case 0x9C: // SBC A, H
            break;
        case 0x9D: // SBC A, L
            break;
        case 0x9E: // SBC A, (HL)
            break;
        case 0x9F: // SBC A, A
            break;
        case 0xA0: // AND B
            cpu.a = cpu.a & cpu.b;
            SET_AND_FLAGS();
            break;
        case 0xA1: // AND C
            cpu.a = cpu.a & cpu.c;
            SET_AND_FLAGS();
            break;
        case 0xA2: // AND D
            cpu.a = cpu.a & cpu.d;
            SET_AND_FLAGS();
            break;
        case 0xA3: // AND E
            cpu.a = cpu.a & cpu.e;
            SET_AND_FLAGS();
            break;
        case 0xA4: // AND H
            cpu.a = cpu.a & cpu.h;
            SET_AND_FLAGS();
            break;
        case 0xA5: // AND L
            cpu.a = cpu.a & cpu.l;
            SET_AND_FLAGS();
            break;
        case 0xA6: // AND (HL)
            // todo implement me
            SET_AND_FLAGS();
            break;
        case 0xA7: // AND A
            cpu.a &= cpu.a;
            SET_AND_FLAGS();
            break;
        case 0xA8: // XOR B
            cpu.a = cpu.a ^ cpu.b;
            SET_xOR_FLAGS();
            break;
        case 0xA9: // XOR C
            cpu.a = cpu.a ^ cpu.c;
            SET_xOR_FLAGS();
            break;
        case 0xAA: // XOR D
            cpu.a = cpu.a ^ cpu.d;
            SET_xOR_FLAGS();
            break;
        case 0xAB: // XOR E
            cpu.a = cpu.a ^ cpu.e;
            SET_xOR_FLAGS();
            break;
        case 0xAC: // XOR H
            cpu.a = cpu.a ^ cpu.h;
            SET_xOR_FLAGS();
            break;
        case 0xAD: // XOR L
            cpu.a = cpu.a ^ cpu.l;
            SET_xOR_FLAGS();
            break;
        case 0xAE: // XOR (HL)
            // todo implement me
            SET_xOR_FLAGS();
            break;
        case 0xAF: // XOR A
            cpu.a ^= cpu.a;
            SET_xOR_FLAGS();
            break;
        case 0xB0: // OR B
            cpu.a = cpu.a | cpu.b;
            SET_xOR_FLAGS();
            break;
        case 0xB1: // OR C
            cpu.a = cpu.a | cpu.c;
            SET_xOR_FLAGS();
            break;
        case 0xB2: // OR D
            cpu.a = cpu.a | cpu.d;
            SET_xOR_FLAGS();
            break;
        case 0xB3: // OR E
            cpu.a = cpu.a | cpu.e;
            SET_xOR_FLAGS();
            break;
        case 0xB4: // OR H
            cpu.a = cpu.a | cpu.h;
            SET_xOR_FLAGS();
            break;
        case 0xB5: // OR L
            cpu.a = cpu.a | cpu.l;
            SET_xOR_FLAGS();
            break;
        case 0xB6: // OR (HL)
            // todo implement me
            SET_xOR_FLAGS();
            break;
        case 0xB7: // OR A
            cpu.a |= cpu.a;
            SET_xOR_FLAGS();
            break;
        case 0xB8: // CP B
            break;
        case 0xB9: // CP C
            break;
        case 0xBA: // CP D
            break;
        case 0xBB: // CP E
            break;
        case 0xBC: // CP H
            break;
        case 0xBD: // CP L
            break;
        case 0xBE: // CP (HL)
            break;
        case 0xBF: // CP A
            break;
        case 0xC0: // RET NZ
            break;
        case 0xC1: // POP BC
            break;
        case 0xC2: // JP NZ, a16
            break;
        case 0xC3: // JP a16
            break;
        case 0xC4: // CALL NZ, a16
            break;
        case 0xC5: // PUSH BC
            break;
        case 0xC6: // ADD A, d8
            break;
        case 0xC7: // RST 00H
            break;
        case 0xC8: // RET Z
            break;
        case 0xC9: // RET
            break;
        case 0xCA: // JP Z, a16
            break;
        case 0xCB: // PREFIX CB
            cpu_prefix_cb_handle(&cycles);
            break;
        case 0xCC: // CALL Z, a16
            break;
        case 0xCD: // CALL a16
            break;
        case 0xCE: // ADC A, d8
            break;
        case 0xCF: // RST 08H
            break;
        case 0xD0: // RET NC
            break;
        case 0xD1: // POP DE
            break;
        case 0xD2: // JP NC, a16
            break;
        case 0xD4: // CALL NC, a16
            break;
        case 0xD5: // PUSH DE
            break;
        case 0xD6: // SUB d8
            break;
        case 0xD7: // RST 10H
            break;
        case 0xD8: // RET C
            break;
        case 0xD9: // RETI
            break;
        case 0xDA: // JP C, a16
            break;
        case 0xDC: // CALL C, a16
            break;
        case 0xDE: // SBC A, d8
            break;
        case 0xDF: // RST 18H
            break;
        case 0xE0: // LDH (a8), A
            break;
        case 0xE1: // POP HL
            break;
        case 0xE2: // LD (C), A
            break;
        case 0xE5: // PUSH HL
            break;
        case 0xE6: // AND d8
            SET_AND_FLAGS();
            break;
        case 0xE7: // RST 20H
            break;
        case 0xE8: // ADD SP, r8
            break;
        case 0xE9: // JP (HL)
            break;
        case 0xEA: // LD (a16), A
            break;
        case 0xEE: // XOR d8
            SET_xOR_FLAGS();
            break;
        case 0xEF: // RST 28H
            break;
        case 0xF0: // LDH A, (a8)
            break;
        case 0xF1: // POP AF
            break;
        case 0xF2: // LD A, (C)
            break;
        case 0xF3: // DI
            break;
        case 0xF5: // PUSH AF
            break;
        case 0xF6: // OR d8
            break;
        case 0xF7: // RST 30H
            break;
        case 0xF8: // LD HL, SP+r8
            break;
        case 0xF9: // LD SP, HL
            break;
        case 0xFA: // LD A, (a16)
            break;
        case 0xFB: // EI
            break;
        case 0xFE: // CP d8
            break;
        case 0xFF: // RST 38H
            break;
        default:
            println("UNKNOWN INSTRUCTION AT 0x%04x", cpu.pc);
            break;
    }

    cpu_dump_state();
}

static void cpu_dump_state() {
    println("CPU:\n Flags:\tZ:%d N:%d H:%d C:%d", GET_FLAG(Z), GET_FLAG(N), GET_FLAG(H), GET_FLAG(C));
    println("\tPC:0x%04x SP:0x%04x", cpu.pc, cpu.sp);
    println("REGISTERS:\n A:0x%02x B:0x%02x C:0x%02x D:0x%02x E:0x%02x H:0x%02x L:0x%02x F:0x%02x",
            cpu.a, cpu.b, cpu.c, cpu.d, cpu.e, cpu.h, cpu.l, cpu.f);
}

static uint8_t cpu_read_register(uint16_t addr) {
    return 0;
}

static void cpu_write_register(uint16_t addr, uint8_t val) {

}