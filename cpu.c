#include "common.h"
#include "cpu.h"
#include "gpu.h"
#include "joypad.h"
#include "rom.h"
#include "sound.h"

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

// I assume that cpu_state is `cpu` object
#define GET_FLAG(flag)   ((cpu.f >> (flag)) & 0x1)

#define SET_FLAGS(zflag, nflag, hflag, cflag) \
                    ((!!(zflag) << Z) | (!!(nflag) << N) | (!!(hflag) << H) | (!!(cflag) << C) | 0)

// TODO: fixme: H flag set
#define SET_INC_FLAGS(data) (cpu.f = SET_FLAGS(data == 0, 0, ((data & 0x0F) == 0x00), GET_FLAG(C)))
#define SET_DEC_FLAGS(data) (cpu.f = SET_FLAGS(data == 0, 1, ((data & 0x0F) == 0x0F), GET_FLAG(C)))
#define SET_AND_FLAGS() (cpu.f = SET_FLAGS(cpu.a == 0, 0, 1, 0))
#define SET_xOR_FLAGS() (cpu.f = SET_FLAGS(cpu.a == 0, 0, 0, 0))

// bit ops flags
#define SET_BIT_FLAGS(bit, reg) (cpu.f = SET_FLAGS(!(reg & (0x1 << bit)), 0, 1, GET_FLAG(C)))
#define SET_SWAP_FLAGS(data) (cpu.f = SET_FLAGS(data == 0, 0, 0, 0))


static void cpu_dump_state ();

static int cpu_step_real (void);

static void handle_interrupts (void);

static uint8_t cpu_read_register (uint16_t addr);

static void cpu_write_register (uint16_t addr, uint8_t val);

static inline ALWAYS_INLINE uint8_t read_byte (uint16_t addr);

static inline ALWAYS_INLINE void write_byte (uint16_t addr, uint8_t val);

static inline ALWAYS_INLINE uint16_t read_word (uint16_t addr);

static inline ALWAYS_INLINE void write_word (uint16_t addr, uint16_t val);

static inline ALWAYS_INLINE void stack_push (uint16_t val);

static inline ALWAYS_INLINE uint16_t stack_pop (void);

static uint8_t serial_read (void);

static void serial_write (uint8_t data);

static void serial_write_control (uint8_t data);

static enum registers map_register (uint8_t opcode);

static void cpu_print_mem (uint16_t begin, uint16_t end);

static void cpu_opcode_bit (enum registers reg, uint8_t bit);

static inline void cpu_opcode_set (enum registers reg, uint8_t bit);

static inline void cpu_opcode_res (enum registers reg, uint8_t bit);

static inline void cpu_opcode_swap(enum registers reg);

static inline uint8_t cpu_opcode_rl(uint8_t data);

static inline void cpu_opcode_rla ();

static inline void cpu_opcode_rl_full (enum registers reg);

static inline uint8_t cpu_opcode_rr (uint8_t data);

static inline void cpu_opcode_rra ();

static inline void cpu_opcode_rr_full (enum registers reg);

static inline void cpu_opcode_sla_full (enum registers reg);

static inline void cpu_opcode_srl_full (enum registers reg);

static inline void cpu_opcode_sra_full (enum registers reg);

static inline uint8_t cpu_opcode_rlc (uint8_t value);

static inline uint8_t cpu_opcode_rrc (uint8_t value);

static inline uint8_t cpu_opcode_sla (uint8_t value);

static inline uint8_t cpu_opcode_srl (uint8_t value);

static inline uint8_t cpu_opcode_sra (uint8_t value);

static inline void cpu_opcode_rlca ();

static inline void cpu_opcode_rrca ();

static inline void cpu_opcode_rlc_full (enum registers reg);

static inline void cpu_opcode_rrc_full (enum registers reg);

static void cpu_prefix_cb_handle (int *cycles);

static inline void cpu_opcode_daa();

static inline void cpu_opcode_add_a(uint8_t value);

static inline void cpu_opcode_add_a_ptr_hl();

static inline void cpu_opcode_add_a_d8();

static inline void cpu_opcode_adc_a(uint8_t value);

static inline void cpu_opcode_adc_a_ptr_hl();

static inline void cpu_opcode_adc_a_d8();

static inline void cpu_opcode_sub_a(uint8_t value);

static inline void cpu_opcode_sub_a_ptr_hl();

static inline void cpu_opcode_sub_a_ptr_d8();

static inline void cpu_opcode_sbc_a(uint8_t value);

static inline void cpu_opcode_sbc_a_ptr_hl();

static inline void cpu_opcode_sbc_a_ptr_d8();

static inline void cpu_opcode_cp_a(uint8_t value);

static inline void cpu_opcode_cp_a_ptr_hl();

static inline void cpu_opcode_cp_a_ptr_d8();

static inline void cpu_opcode_add_hl(uint16_t value);

static inline void cpu_opcode_add_sp(int8_t value);

static inline void cpu_opcode_ccf ();

static inline void cpu_opcode_cpl ();

static inline void cpu_opcode_scf ();

static inline void cpu_opcode_ld_hl_sp (int8_t value);

static inline void cpu_opcode_rst (const uint8_t offset);

static inline void cpu_opcode_interrupt (const uint8_t offset);

static void cpu_instr_0x00 (int *cycles);
static void cpu_instr_0x01 (int *cycles);
static void cpu_instr_0x02 (int *cycles);
static void cpu_instr_0x03 (int *cycles);
static void cpu_instr_0x04 (int *cycles);
static void cpu_instr_0x05 (int *cycles);
static void cpu_instr_0x06 (int *cycles);
static void cpu_instr_0x07 (int *cycles);
static void cpu_instr_0x08 (int *cycles);
static void cpu_instr_0x09(int *cycles);
static void cpu_instr_0x0a(int *cycles);
static void cpu_instr_0x0b(int *cycles);
static void cpu_instr_0x0c(int *cycles);
static void cpu_instr_0x0d(int *cycles);
static void cpu_instr_0x0e(int *cycles);
static void cpu_instr_0x0f(int *cycles);
static void cpu_instr_0x10(int *cycles);
static void cpu_instr_0x11(int *cycles);
static void cpu_instr_0x12(int *cycles);
static void cpu_instr_0x13(int *cycles);
static void cpu_instr_0x14(int *cycles);
static void cpu_instr_0x15(int *cycles);
static void cpu_instr_0x16(int *cycles);
static void cpu_instr_0x17(int *cycles);
static void cpu_instr_0x18(int *cycles);
static void cpu_instr_0x19(int *cycles);
static void cpu_instr_0x1a(int *cycles);
static void cpu_instr_0x1b(int *cycles);
static void cpu_instr_0x1c(int *cycles);
static void cpu_instr_0x1d(int *cycles);
static void cpu_instr_0x1e(int *cycles);
static void cpu_instr_0x1f(int *cycles);
static void cpu_instr_0x20(int *cycles);
static void cpu_instr_0x21(int *cycles);
static void cpu_instr_0x22(int *cycles);
static void cpu_instr_0x23(int *cycles);
static void cpu_instr_0x24(int *cycles);
static void cpu_instr_0x25(int *cycles);
static void cpu_instr_0x26(int *cycles);
static void cpu_instr_0x27(int *cycles);
static void cpu_instr_0x28(int *cycles);
static void cpu_instr_0x29(int *cycles);
static void cpu_instr_0x2a(int *cycles);
static void cpu_instr_0x2b(int *cycles);
static void cpu_instr_0x2c(int *cycles);
static void cpu_instr_0x2d(int *cycles);
static void cpu_instr_0x2e(int *cycles);
static void cpu_instr_0x2f(int *cycles);
static void cpu_instr_0x30(int *cycles);
static void cpu_instr_0x31(int *cycles);
static void cpu_instr_0x32(int *cycles);
static void cpu_instr_0x33(int *cycles);
static void cpu_instr_0x34(int *cycles);
static void cpu_instr_0x35(int *cycles);
static void cpu_instr_0x36(int *cycles);
static void cpu_instr_0x37(int *cycles);
static void cpu_instr_0x38(int *cycles);
static void cpu_instr_0x39(int *cycles);
static void cpu_instr_0x3a(int *cycles);
static void cpu_instr_0x3b(int *cycles);
static void cpu_instr_0x3c(int *cycles);
static void cpu_instr_0x3d(int *cycles);
static void cpu_instr_0x3e(int *cycles);
static void cpu_instr_0x3f(int *cycles);
static void cpu_instr_0x40(int *cycles);
static void cpu_instr_0x41(int *cycles);
static void cpu_instr_0x42(int *cycles);
static void cpu_instr_0x43(int *cycles);
static void cpu_instr_0x44(int *cycles);
static void cpu_instr_0x45(int *cycles);
static void cpu_instr_0x46(int *cycles);
static void cpu_instr_0x47(int *cycles);
static void cpu_instr_0x48(int *cycles);
static void cpu_instr_0x49(int *cycles);
static void cpu_instr_0x4a(int *cycles);
static void cpu_instr_0x4b(int *cycles);
static void cpu_instr_0x4c(int *cycles);
static void cpu_instr_0x4d(int *cycles);
static void cpu_instr_0x4e(int *cycles);
static void cpu_instr_0x4f(int *cycles);
static void cpu_instr_0x50(int *cycles);
static void cpu_instr_0x51(int *cycles);
static void cpu_instr_0x52(int *cycles);
static void cpu_instr_0x53(int *cycles);
static void cpu_instr_0x54(int *cycles);
static void cpu_instr_0x55(int *cycles);
static void cpu_instr_0x56(int *cycles);
static void cpu_instr_0x57(int *cycles);
static void cpu_instr_0x58(int *cycles);
static void cpu_instr_0x59(int *cycles);
static void cpu_instr_0x5a(int *cycles);
static void cpu_instr_0x5b(int *cycles);
static void cpu_instr_0x5c(int *cycles);
static void cpu_instr_0x5d(int *cycles);
static void cpu_instr_0x5e(int *cycles);
static void cpu_instr_0x5f(int *cycles);
static void cpu_instr_0x60(int *cycles);
static void cpu_instr_0x61(int *cycles);
static void cpu_instr_0x62(int *cycles);
static void cpu_instr_0x63(int *cycles);
static void cpu_instr_0x64(int *cycles);
static void cpu_instr_0x65(int *cycles);
static void cpu_instr_0x66(int *cycles);
static void cpu_instr_0x67(int *cycles);
static void cpu_instr_0x68(int *cycles);
static void cpu_instr_0x69(int *cycles);
static void cpu_instr_0x6a(int *cycles);
static void cpu_instr_0x6b(int *cycles);
static void cpu_instr_0x6c(int *cycles);
static void cpu_instr_0x6d(int *cycles);
static void cpu_instr_0x6e(int *cycles);
static void cpu_instr_0x6f(int *cycles);
static void cpu_instr_0x70(int *cycles);
static void cpu_instr_0x71(int *cycles);
static void cpu_instr_0x72(int *cycles);
static void cpu_instr_0x73(int *cycles);
static void cpu_instr_0x74(int *cycles);
static void cpu_instr_0x75(int *cycles);
static void cpu_instr_0x76(int *cycles);
static void cpu_instr_0x77(int *cycles);
static void cpu_instr_0x78(int *cycles);
static void cpu_instr_0x79(int *cycles);
static void cpu_instr_0x7a(int *cycles);
static void cpu_instr_0x7b(int *cycles);
static void cpu_instr_0x7c(int *cycles);
static void cpu_instr_0x7d(int *cycles);
static void cpu_instr_0x7e(int *cycles);
static void cpu_instr_0x7f(int *cycles);
static void cpu_instr_0x80(int *cycles);
static void cpu_instr_0x81(int *cycles);
static void cpu_instr_0x82(int *cycles);
static void cpu_instr_0x83(int *cycles);
static void cpu_instr_0x84(int *cycles);
static void cpu_instr_0x85(int *cycles);
static void cpu_instr_0x86(int *cycles);
static void cpu_instr_0x87(int *cycles);
static void cpu_instr_0x88(int *cycles);
static void cpu_instr_0x89(int *cycles);
static void cpu_instr_0x8a(int *cycles);
static void cpu_instr_0x8b(int *cycles);
static void cpu_instr_0x8c(int *cycles);
static void cpu_instr_0x8d(int *cycles);
static void cpu_instr_0x8e(int *cycles);
static void cpu_instr_0x8f(int *cycles);
static void cpu_instr_0x90(int *cycles);
static void cpu_instr_0x91(int *cycles);
static void cpu_instr_0x92(int *cycles);
static void cpu_instr_0x93(int *cycles);
static void cpu_instr_0x94(int *cycles);
static void cpu_instr_0x95(int *cycles);
static void cpu_instr_0x96(int *cycles);
static void cpu_instr_0x97(int *cycles);
static void cpu_instr_0x98(int *cycles);
static void cpu_instr_0x99(int *cycles);
static void cpu_instr_0x9a(int *cycles);
static void cpu_instr_0x9b(int *cycles);
static void cpu_instr_0x9c(int *cycles);
static void cpu_instr_0x9d(int *cycles);
static void cpu_instr_0x9e(int *cycles);
static void cpu_instr_0x9f(int *cycles);
static void cpu_instr_0xa0(int *cycles);
static void cpu_instr_0xa1(int *cycles);
static void cpu_instr_0xa2(int *cycles);
static void cpu_instr_0xa3(int *cycles);
static void cpu_instr_0xa4(int *cycles);
static void cpu_instr_0xa5(int *cycles);
static void cpu_instr_0xa6(int *cycles);
static void cpu_instr_0xa7(int *cycles);
static void cpu_instr_0xa8(int *cycles);
static void cpu_instr_0xa9(int *cycles);
static void cpu_instr_0xaa(int *cycles);
static void cpu_instr_0xab(int *cycles);
static void cpu_instr_0xac(int *cycles);
static void cpu_instr_0xad(int *cycles);
static void cpu_instr_0xae(int *cycles);
static void cpu_instr_0xaf(int *cycles);
static void cpu_instr_0xb0(int *cycles);
static void cpu_instr_0xb1(int *cycles);
static void cpu_instr_0xb2(int *cycles);
static void cpu_instr_0xb3(int *cycles);
static void cpu_instr_0xb4(int *cycles);
static void cpu_instr_0xb5(int *cycles);
static void cpu_instr_0xb6(int *cycles);
static void cpu_instr_0xb7(int *cycles);
static void cpu_instr_0xb8(int *cycles);
static void cpu_instr_0xb9(int *cycles);
static void cpu_instr_0xba(int *cycles);
static void cpu_instr_0xbb(int *cycles);
static void cpu_instr_0xbc(int *cycles);
static void cpu_instr_0xbd(int *cycles);
static void cpu_instr_0xbe(int *cycles);
static void cpu_instr_0xbf(int *cycles);
static void cpu_instr_0xc0(int *cycles);
static void cpu_instr_0xc1(int *cycles);
static void cpu_instr_0xc2(int *cycles);
static void cpu_instr_0xc3(int *cycles);
static void cpu_instr_0xc4(int *cycles);
static void cpu_instr_0xc5(int *cycles);
static void cpu_instr_0xc6(int *cycles);
static void cpu_instr_0xc7(int *cycles);
static void cpu_instr_0xc8(int *cycles);
static void cpu_instr_0xc9(int *cycles);
static void cpu_instr_0xca(int *cycles);
static void cpu_instr_0xcb(int *cycles);
static void cpu_instr_0xcc(int *cycles);
static void cpu_instr_0xcd(int *cycles);
static void cpu_instr_0xce(int *cycles);
static void cpu_instr_0xcf(int *cycles);
static void cpu_instr_0xd0(int *cycles);
static void cpu_instr_0xd1(int *cycles);
static void cpu_instr_0xd2(int *cycles);
static void cpu_instr_0xd3(int *cycles);
static void cpu_instr_0xd4(int *cycles);
static void cpu_instr_0xd5(int *cycles);
static void cpu_instr_0xd6(int *cycles);
static void cpu_instr_0xd7(int *cycles);
static void cpu_instr_0xd8(int *cycles);
static void cpu_instr_0xd9(int *cycles);
static void cpu_instr_0xda(int *cycles);
static void cpu_instr_0xdb(int *cycles);
static void cpu_instr_0xdc(int *cycles);
static void cpu_instr_0xdd(int *cycles);
static void cpu_instr_0xde(int *cycles);
static void cpu_instr_0xdf(int *cycles);
static void cpu_instr_0xe0(int *cycles);
static void cpu_instr_0xe1(int *cycles);
static void cpu_instr_0xe2(int *cycles);
static void cpu_instr_0xe3(int *cycles);
static void cpu_instr_0xe4(int *cycles);
static void cpu_instr_0xe5(int *cycles);
static void cpu_instr_0xe6(int *cycles);
static void cpu_instr_0xe7(int *cycles);
static void cpu_instr_0xe8(int *cycles);
static void cpu_instr_0xe9(int *cycles);
static void cpu_instr_0xea(int *cycles);
static void cpu_instr_0xeb(int *cycles);
static void cpu_instr_0xec(int *cycles);
static void cpu_instr_0xed(int *cycles);
static void cpu_instr_0xee(int *cycles);
static void cpu_instr_0xef(int *cycles);
static void cpu_instr_0xf0(int *cycles);
static void cpu_instr_0xf1(int *cycles);
static void cpu_instr_0xf2(int *cycles);
static void cpu_instr_0xf3(int *cycles);
static void cpu_instr_0xf4(int *cycles);
static void cpu_instr_0xf5(int *cycles);
static void cpu_instr_0xf6(int *cycles);
static void cpu_instr_0xf7(int *cycles);
static void cpu_instr_0xf8(int *cycles);
static void cpu_instr_0xf9(int *cycles);
static void cpu_instr_0xfa(int *cycles);
static void cpu_instr_0xfb(int *cycles);
static void cpu_instr_0xfc(int *cycles);
static void cpu_instr_0xfd(int *cycles);
static void cpu_instr_0xfe(int *cycles);
static void cpu_instr_0xff(int *cycles);

typedef struct {
	struct {
		union {
			struct {
				uint8_t f;
				uint8_t a;
			};
			uint16_t af;
		};
	};
	struct {
		union {
			struct {
				uint8_t c;
				uint8_t b;
			};
			uint16_t bc;
		};
	};
	struct {
		union {
			struct {
				uint8_t e;
				uint8_t d;
			};
			uint16_t de;
		};
	};
	struct {
		union {
			struct {
				uint8_t l;
				uint8_t h;
			};
			uint16_t hl;
		};
	};
	uint16_t sp;
	uint16_t pc;

	bool stop;

	bool boot_rom_enabled;

	bool ime;

	uint8_t interrupt_flag;

	uint8_t interrupt_enable;
} cpu_state;

// this is a table for cycles count for each instruction
// we might modify cycles counter in cpu_step()
static int cycles_main_opcodes[256] = {
	4, 12, 8, 8, 4, 4, 8, 4, 20, 8, 8, 8, 4, 4, 8, 4,
	4, 12, 8, 8, 4, 4, 8, 4, 12, 8, 8, 8, 4, 4, 8, 4,
	8, 12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,
	8, 12, 8, 8, 12, 12, 12, 4, 8, 8, 8, 8, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	8, 8, 8, 8, 8, 8, 4, 8, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,
	8, 12, 12, 16, 12, 16, 8, 16, 8, 16, 12, 4, 12, 24, 8, 16,
	8, 12, 12, 0, 12, 16, 8, 16, 8, 16, 12, 0, 12, 0, 8, 16,
	12, 12, 8, 0, 0, 16, 8, 16, 16, 4, 16, 0, 0, 0, 8, 16,
	12, 12, 8, 4, 0, 16, 8, 16, 12, 8, 16, 4, 0, 0, 8, 16
};

static int cycles_0xCB_opcodes[256] = {
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8,
	8, 8, 8, 8, 8, 8, 16, 8, 8, 8, 8, 8, 8, 8, 16, 8
};

typedef void (*instruction_handler)(int *cycles);

static instruction_handler instructions[256] = {
	cpu_instr_0x00, cpu_instr_0x01, cpu_instr_0x02, cpu_instr_0x03, cpu_instr_0x04,
	cpu_instr_0x05, cpu_instr_0x06, cpu_instr_0x07, cpu_instr_0x08, cpu_instr_0x09,
	cpu_instr_0x0a, cpu_instr_0x0b, cpu_instr_0x0c, cpu_instr_0x0d, cpu_instr_0x0e,
	cpu_instr_0x0f, cpu_instr_0x10, cpu_instr_0x11, cpu_instr_0x12, cpu_instr_0x13,
	cpu_instr_0x14, cpu_instr_0x15, cpu_instr_0x16, cpu_instr_0x17, cpu_instr_0x18,
	cpu_instr_0x19, cpu_instr_0x1a, cpu_instr_0x1b, cpu_instr_0x1c, cpu_instr_0x1d,
	cpu_instr_0x1e, cpu_instr_0x1f, cpu_instr_0x20, cpu_instr_0x21, cpu_instr_0x22,
	cpu_instr_0x23, cpu_instr_0x24, cpu_instr_0x25, cpu_instr_0x26, cpu_instr_0x27,
	cpu_instr_0x28, cpu_instr_0x29, cpu_instr_0x2a, cpu_instr_0x2b, cpu_instr_0x2c,
	cpu_instr_0x2d, cpu_instr_0x2e, cpu_instr_0x2f, cpu_instr_0x30, cpu_instr_0x31,
	cpu_instr_0x32, cpu_instr_0x33, cpu_instr_0x34, cpu_instr_0x35, cpu_instr_0x36,
	cpu_instr_0x37, cpu_instr_0x38, cpu_instr_0x39, cpu_instr_0x3a, cpu_instr_0x3b,
	cpu_instr_0x3c, cpu_instr_0x3d, cpu_instr_0x3e, cpu_instr_0x3f, cpu_instr_0x40,
	cpu_instr_0x41, cpu_instr_0x42, cpu_instr_0x43, cpu_instr_0x44, cpu_instr_0x45,
	cpu_instr_0x46, cpu_instr_0x47, cpu_instr_0x48, cpu_instr_0x49, cpu_instr_0x4a,
	cpu_instr_0x4b, cpu_instr_0x4c, cpu_instr_0x4d, cpu_instr_0x4e, cpu_instr_0x4f,
	cpu_instr_0x50, cpu_instr_0x51, cpu_instr_0x52, cpu_instr_0x53, cpu_instr_0x54,
	cpu_instr_0x55, cpu_instr_0x56, cpu_instr_0x57, cpu_instr_0x58, cpu_instr_0x59,
	cpu_instr_0x5a, cpu_instr_0x5b, cpu_instr_0x5c, cpu_instr_0x5d, cpu_instr_0x5e,
	cpu_instr_0x5f, cpu_instr_0x60, cpu_instr_0x61, cpu_instr_0x62, cpu_instr_0x63,
	cpu_instr_0x64, cpu_instr_0x65, cpu_instr_0x66, cpu_instr_0x67, cpu_instr_0x68,
	cpu_instr_0x69, cpu_instr_0x6a, cpu_instr_0x6b, cpu_instr_0x6c, cpu_instr_0x6d,
	cpu_instr_0x6e, cpu_instr_0x6f, cpu_instr_0x70, cpu_instr_0x71, cpu_instr_0x72,
	cpu_instr_0x73, cpu_instr_0x74, cpu_instr_0x75, cpu_instr_0x76, cpu_instr_0x77,
	cpu_instr_0x78, cpu_instr_0x79, cpu_instr_0x7a, cpu_instr_0x7b, cpu_instr_0x7c,
	cpu_instr_0x7d, cpu_instr_0x7e, cpu_instr_0x7f, cpu_instr_0x80, cpu_instr_0x81,
	cpu_instr_0x82, cpu_instr_0x83, cpu_instr_0x84, cpu_instr_0x85, cpu_instr_0x86,
	cpu_instr_0x87, cpu_instr_0x88, cpu_instr_0x89, cpu_instr_0x8a, cpu_instr_0x8b,
	cpu_instr_0x8c, cpu_instr_0x8d, cpu_instr_0x8e, cpu_instr_0x8f, cpu_instr_0x90,
	cpu_instr_0x91, cpu_instr_0x92, cpu_instr_0x93, cpu_instr_0x94, cpu_instr_0x95,
	cpu_instr_0x96, cpu_instr_0x97, cpu_instr_0x98, cpu_instr_0x99, cpu_instr_0x9a,
	cpu_instr_0x9b, cpu_instr_0x9c, cpu_instr_0x9d, cpu_instr_0x9e, cpu_instr_0x9f,
	cpu_instr_0xa0, cpu_instr_0xa1, cpu_instr_0xa2, cpu_instr_0xa3, cpu_instr_0xa4,
	cpu_instr_0xa5, cpu_instr_0xa6, cpu_instr_0xa7, cpu_instr_0xa8, cpu_instr_0xa9,
	cpu_instr_0xaa, cpu_instr_0xab, cpu_instr_0xac, cpu_instr_0xad, cpu_instr_0xae,
	cpu_instr_0xaf, cpu_instr_0xb0, cpu_instr_0xb1, cpu_instr_0xb2, cpu_instr_0xb3,
	cpu_instr_0xb4, cpu_instr_0xb5, cpu_instr_0xb6, cpu_instr_0xb7, cpu_instr_0xb8,
	cpu_instr_0xb9, cpu_instr_0xba, cpu_instr_0xbb, cpu_instr_0xbc, cpu_instr_0xbd,
	cpu_instr_0xbe, cpu_instr_0xbf, cpu_instr_0xc0, cpu_instr_0xc1, cpu_instr_0xc2,
	cpu_instr_0xc3, cpu_instr_0xc4, cpu_instr_0xc5, cpu_instr_0xc6, cpu_instr_0xc7,
	cpu_instr_0xc8, cpu_instr_0xc9, cpu_instr_0xca, cpu_instr_0xcb, cpu_instr_0xcc,
	cpu_instr_0xcd, cpu_instr_0xce, cpu_instr_0xcf, cpu_instr_0xd0, cpu_instr_0xd1,
	cpu_instr_0xd2, cpu_instr_0xd3, cpu_instr_0xd4, cpu_instr_0xd5, cpu_instr_0xd6,
	cpu_instr_0xd7, cpu_instr_0xd8, cpu_instr_0xd9, cpu_instr_0xda, cpu_instr_0xdb,
	cpu_instr_0xdc, cpu_instr_0xdd, cpu_instr_0xde, cpu_instr_0xdf, cpu_instr_0xe0,
	cpu_instr_0xe1, cpu_instr_0xe2, cpu_instr_0xe3, cpu_instr_0xe4, cpu_instr_0xe5,
	cpu_instr_0xe6, cpu_instr_0xe7, cpu_instr_0xe8, cpu_instr_0xe9, cpu_instr_0xea,
	cpu_instr_0xeb, cpu_instr_0xec, cpu_instr_0xed, cpu_instr_0xee, cpu_instr_0xef,
	cpu_instr_0xf0, cpu_instr_0xf1, cpu_instr_0xf2, cpu_instr_0xf3, cpu_instr_0xf4,
	cpu_instr_0xf5, cpu_instr_0xf6, cpu_instr_0xf7, cpu_instr_0xf8, cpu_instr_0xf9,
	cpu_instr_0xfa, cpu_instr_0xfb, cpu_instr_0xfc, cpu_instr_0xfd, cpu_instr_0xfe,
	cpu_instr_0xff
};

// CPU STATE
static cpu_state cpu;

// ALL KINDS OF MEMORY
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

void cpu_init () {
	cpu.stop             = false;
	cpu.pc               = 0x0000;
	cpu.sp               = 0x0000;
	cpu.ime              = 0;
	cpu.boot_rom_enabled = 1;
	cpu.interrupt_enable = 0;
	cpu.interrupt_flag   = 0xE0;
}



static uint8_t serial_data = 0x0;

static uint8_t serial_read (void) {
	return serial_data;
}

static void serial_write (uint8_t data) {
	serial_data = data;
}

static void serial_write_control (uint8_t data) {
	if (data & (1<<7)) {
		printl("%c", serial_data);
	}
}

static void handle_interrupts (void) {
	if (cpu.ime) {
		uint8_t fired = cpu.interrupt_flag & cpu.interrupt_enable;

		if (!fired) {
			return;
		}

		if (fired & 0x1) {
			cpu_opcode_interrupt(0x40);
			cpu.interrupt_flag &= ~(1<<0);
		}
		else if (fired & 0x2) {
			cpu_opcode_interrupt(0x48);
			cpu.interrupt_flag &= ~(1<<1);
		}
		else if (fired & 0x4) {
			cpu_opcode_interrupt(0x50);
			cpu.interrupt_flag &= ~(1<<2);
		}
		else if (fired & 0x8) {
			cpu_opcode_interrupt(0x58);
			cpu.interrupt_flag &= ~(1<<3);
		}
		else if (fired & 0x10) {
			cpu_opcode_interrupt(0x60);
			cpu.interrupt_flag &= ~(1<<4);
		}
	}
}

uint8_t cpu_get_dma (uint8_t start_addr, uint8_t index) {
	uint16_t addr = (start_addr<<8) | index;
	return read_byte(addr);
}

int cpu_step (void) {
	int cycles = 0;

	if (!cpu.stop) {
		handle_interrupts();

		cycles = cpu_step_real();
	}

	return cycles;
}

static int cpu_step_real (void) {
	uint8_t instr  = read_byte(cpu.pc++);
	int     cycles = cycles_main_opcodes[instr];
	instructions[instr](&cycles);
	return cycles;
}

void cpu_request_interrupt (int bit) {
	cpu.interrupt_flag |= (1 << bit) | 0xE0;
}


static void cpu_instr_0x00(int *cycles) {
	// NOP
}

static void cpu_instr_0x01(int *cycles) {
	// LD BC, d16
	cpu.bc = read_word(cpu.pc);
	cpu.pc += 2;
}

static void cpu_instr_0x02(int *cycles) {
	// LD (BC), A
	write_byte(cpu.bc, cpu.a);
}

static void cpu_instr_0x03(int *cycles) {
	// INC BC
	cpu.bc++;
}

static void cpu_instr_0x04(int *cycles) {
	// INC B
	cpu.b++;
	SET_INC_FLAGS(cpu.b);
}

static void cpu_instr_0x05(int *cycles) {
	// DEC B
	cpu.b--;
	SET_DEC_FLAGS(cpu.b);
}

static void cpu_instr_0x06(int *cycles) {
	// LD B, d8
	cpu.b = read_byte(cpu.pc);
	cpu.pc++;
}

static void cpu_instr_0x07(int *cycles) {
	// RLCA
	cpu_opcode_rlca();
}

static void cpu_instr_0x08(int *cycles) {
	// LD (a16), SP
	write_word(read_word(cpu.pc), cpu.sp);
	cpu.pc += 2;
}

static void cpu_instr_0x09(int *cycles) {
	// ADD HL, BC
	cpu_opcode_add_hl(cpu.bc);
}

static void cpu_instr_0x0a(int *cycles) {
	// LD A, (BC)
	cpu.a = read_byte(cpu.bc);
}

static void cpu_instr_0x0b(int *cycles) {
	// DEC BC
	cpu.bc--;
}

static void cpu_instr_0x0c(int *cycles) {
	// INC C
	cpu.c++;
	SET_INC_FLAGS(cpu.c);
}

static void cpu_instr_0x0d(int *cycles) {
	// DEC C
	cpu.c--;
	SET_DEC_FLAGS(cpu.c);
}

static void cpu_instr_0x0e(int *cycles) {
	// LD C, d8
	cpu.c = read_byte(cpu.pc);
	cpu.pc++;
}

static void cpu_instr_0x0f(int *cycles) {
	// RRCA
	cpu_opcode_rrca();
}

static void cpu_instr_0x10(int *cycles) {
	// STOP
	println("STOP");
}

static void cpu_instr_0x11(int *cycles) {
	// LD DE, d16
	cpu.de = read_word(cpu.pc);
	cpu.pc += 2;
}

static void cpu_instr_0x12(int *cycles) {
	// LD (DE), A
	write_byte(cpu.de, cpu.a);
}

static void cpu_instr_0x13(int *cycles) {
	// INC DE
	cpu.de++;
}

static void cpu_instr_0x14(int *cycles) {
	// INC D
	cpu.d++;
	SET_INC_FLAGS(cpu.d);
}

static void cpu_instr_0x15(int *cycles) {
	// DEC D
	cpu.d--;
	SET_DEC_FLAGS(cpu.d);
}

static void cpu_instr_0x16(int *cycles) {
	// LD D, d8
	cpu.d = read_byte(cpu.pc);
	cpu.pc++;
}

static void cpu_instr_0x17(int *cycles) {
	// RLA
	cpu_opcode_rla();
}

static void cpu_instr_0x18(int *cycles) {
	// JR r8
	cpu.pc = (int16_t) cpu.pc + (int8_t) read_byte(cpu.pc) + 1;
}

static void cpu_instr_0x19(int *cycles) {
	// ADD HL, DE
	cpu_opcode_add_hl(cpu.de);
}

static void cpu_instr_0x1a(int *cycles) {
	// LD A, (DE)
	cpu.a = read_byte(cpu.de);
}

static void cpu_instr_0x1b(int *cycles) {
	// DEC DE
	cpu.de--;
}

static void cpu_instr_0x1c(int *cycles) {
	// INC E
	cpu.e++;
	SET_INC_FLAGS(cpu.e);
}

static void cpu_instr_0x1d(int *cycles) {
	// DEC E
	cpu.e--;
	SET_DEC_FLAGS(cpu.e);
}

static void cpu_instr_0x1e(int *cycles) {
	// LD E, d8
	cpu.e = read_byte(cpu.pc);
	cpu.pc++;
}

static void cpu_instr_0x1f(int *cycles) {
	// RRA
	cpu_opcode_rra();
}

static void cpu_instr_0x20(int *cycles) {
	// JR NZ, r8
	if (!GET_FLAG(Z)) {
		cpu.pc = (int16_t) cpu.pc + (int8_t) read_byte(cpu.pc) + 1;
		*cycles += 4;
	}
	else {
		cpu.pc++;
	}
}

static void cpu_instr_0x21(int *cycles) {
	// LD HL, d16
	cpu.hl = read_word(cpu.pc);
	cpu.pc += 2;
}

static void cpu_instr_0x22(int *cycles) {
	// LD (HL+), A
	write_byte(cpu.hl++, cpu.a);
}

static void cpu_instr_0x23(int *cycles) {
	// INC HL
	cpu.hl++;
}

static void cpu_instr_0x24(int *cycles) {
	// INC H
	cpu.h++;
	SET_INC_FLAGS(cpu.h);
}

static void cpu_instr_0x25(int *cycles) {
	// DEC H
	cpu.h--;
	SET_DEC_FLAGS(cpu.h);
}

static void cpu_instr_0x26(int *cycles) {
	// LD H, d8
	cpu.h = read_byte(cpu.pc);
	cpu.pc++;
}

static void cpu_instr_0x27(int *cycles) {
	// DAA
	cpu_opcode_daa();
}

static void cpu_instr_0x28(int *cycles) {
	// JR Z, r8
	if (GET_FLAG(Z)) {
		cpu.pc = (int16_t) cpu.pc + (int8_t) read_byte(cpu.pc) + 1;
		*cycles += 4;
	}
	else {
		cpu.pc++;
	}
}

static void cpu_instr_0x29(int *cycles) {
	// ADD HL, HL
	cpu_opcode_add_hl(cpu.hl);
}

static void cpu_instr_0x2a(int *cycles) {
	// LD A, (HL+)
	cpu.a = read_byte(cpu.hl++);
}

static void cpu_instr_0x2b(int *cycles) {
	// DEC HL
	cpu.hl--;
}

static void cpu_instr_0x2c(int *cycles) {
	// INC L
	cpu.l++;
	SET_INC_FLAGS(cpu.l);
}

static void cpu_instr_0x2d(int *cycles) {
	// DEC L
	cpu.l--;
	SET_DEC_FLAGS(cpu.l);
}

static void cpu_instr_0x2e(int *cycles) {
	// LD L, d8
	cpu.l = read_byte(cpu.pc);
	cpu.pc++;
}

static void cpu_instr_0x2f(int *cycles) {
	// CPL
	cpu_opcode_cpl();
}

static void cpu_instr_0x30(int *cycles) {
	// JR NC, r8
	if (!GET_FLAG(C)) {
		cpu.pc = (int16_t) cpu.pc + (int8_t) read_byte(cpu.pc) + 1;
		*cycles += 4;
	}
	else {
		cpu.pc++;
	}
}

static void cpu_instr_0x31(int *cycles) {
	// LD SP, d16
	cpu.sp = read_word(cpu.pc);
	cpu.pc += 2;
}

static void cpu_instr_0x32(int *cycles) {
	// LD (HL-), A
	write_byte(cpu.hl--, cpu.a);
}

static void cpu_instr_0x33(int *cycles) {
	// INC SP
	cpu.sp++;
}

static void cpu_instr_0x34(int *cycles) {
	// INC (HL)
	write_byte(cpu.hl, read_byte(cpu.hl) + 1);
	SET_INC_FLAGS(read_byte(cpu.hl));
}

static void cpu_instr_0x35(int *cycles) {
	// DEC (HL)
	write_byte(cpu.hl, read_byte(cpu.hl) - 1);
	SET_DEC_FLAGS(read_byte(cpu.hl));
}

static void cpu_instr_0x36(int *cycles) {
	// LD (HL), d8
	write_byte(cpu.hl, read_byte(cpu.pc));
	cpu.pc++;
}

static void cpu_instr_0x37(int *cycles) {
	// SCF
	cpu_opcode_scf();
}

static void cpu_instr_0x38(int *cycles) {
	// JR C, r8
	if (GET_FLAG(C)) {
		cpu.pc = (int16_t) cpu.pc + (int8_t) read_byte(cpu.pc) + 1;
		*cycles += 4;
	}
	else {
		cpu.pc++;
	}
}

static void cpu_instr_0x39(int *cycles) {
	// ADD HL, SP
	cpu_opcode_add_hl(cpu.sp);
}

static void cpu_instr_0x3a(int *cycles) {
	// LD A, (HL-)
	cpu.a = read_byte(cpu.hl--);
}

static void cpu_instr_0x3b(int *cycles) {
	// DEC SP
	cpu.sp--;
}

static void cpu_instr_0x3c(int *cycles) {
	// INC A
	cpu.a++;
	SET_INC_FLAGS(cpu.a);
}

static void cpu_instr_0x3d(int *cycles) {
	// DEC A
	cpu.a--;
	SET_DEC_FLAGS(cpu.a);
}

static void cpu_instr_0x3e(int *cycles) {
	// LD A, d8
	cpu.a = read_byte(cpu.pc);
	cpu.pc++;
}

static void cpu_instr_0x3f(int *cycles) {
	// CCF
	cpu_opcode_ccf();
}

static void cpu_instr_0x40(int *cycles) {
	// LD B, B
	cpu.b = cpu.b;
}

static void cpu_instr_0x41(int *cycles) {
	// LD B, C
	cpu.b = cpu.c;
}

static void cpu_instr_0x42(int *cycles) {
	// LD B, D
	cpu.b = cpu.d;
}

static void cpu_instr_0x43(int *cycles) {
	// LD B, E
	cpu.b = cpu.e;
}

static void cpu_instr_0x44(int *cycles) {
	// LD B, H
	cpu.b = cpu.h;
}

static void cpu_instr_0x45(int *cycles) {
	// LD B, L
	cpu.b = cpu.l;
}

static void cpu_instr_0x46(int *cycles) {
	// LD B, (HL)
	cpu.b = read_byte(cpu.hl);
}

static void cpu_instr_0x47(int *cycles) {
	// LD B, A
	cpu.b = cpu.a;
}

static void cpu_instr_0x48(int *cycles) {
	// LD C, B
	cpu.c = cpu.b;
}

static void cpu_instr_0x49(int *cycles) {
	// LD C, C
	cpu.c = cpu.c;
}

static void cpu_instr_0x4a(int *cycles) {
	// LD C, D
	cpu.c = cpu.d;
}

static void cpu_instr_0x4b(int *cycles) {
	// LD C, E
	cpu.c = cpu.e;
}

static void cpu_instr_0x4c(int *cycles) {
	// LD C, H
	cpu.c = cpu.h;
}

static void cpu_instr_0x4d(int *cycles) {
	// LD C, L
	cpu.c = cpu.l;
}

static void cpu_instr_0x4e(int *cycles) {
	// LD C, (HL)
	cpu.c = read_byte(cpu.hl);
}

static void cpu_instr_0x4f(int *cycles) {
	// LD C, A
	cpu.c = cpu.a;
}

static void cpu_instr_0x50(int *cycles) {
	// LD D, B
	cpu.d = cpu.b;
}

static void cpu_instr_0x51(int *cycles) {
	// LD D, C
	cpu.d = cpu.c;
}

static void cpu_instr_0x52(int *cycles) {
	// LD D, D
	cpu.d = cpu.d;
}

static void cpu_instr_0x53(int *cycles) {
	// LD D, E
	cpu.d = cpu.e;
}

static void cpu_instr_0x54(int *cycles) {
	// LD D, H
	cpu.d = cpu.h;
}

static void cpu_instr_0x55(int *cycles) {
	// LD D, L
	cpu.d = cpu.l;
}

static void cpu_instr_0x56(int *cycles) {
	// LD D, (HL)
	cpu.d = read_byte(cpu.hl);
}

static void cpu_instr_0x57(int *cycles) {
	// LD D, A
	cpu.d = cpu.a;
}

static void cpu_instr_0x58(int *cycles) {
	// LD E, B
	cpu.e = cpu.b;
}

static void cpu_instr_0x59(int *cycles) {
	// LD E, C
	cpu.e = cpu.c;
}

static void cpu_instr_0x5a(int *cycles) {
	// LD E, D
	cpu.e = cpu.d;
}

static void cpu_instr_0x5b(int *cycles) {
	// LD E, E
	cpu.e = cpu.e;
}

static void cpu_instr_0x5c(int *cycles) {
	// LD E, H
	cpu.e = cpu.h;
}

static void cpu_instr_0x5d(int *cycles) {
	// LD E, L
	cpu.e = cpu.l;
}

static void cpu_instr_0x5e(int *cycles) {
	// LD E, (HL)
	cpu.e = read_byte(cpu.hl);
}

static void cpu_instr_0x5f(int *cycles) {
	// LD E, A
	cpu.e = cpu.a;
}

static void cpu_instr_0x60(int *cycles) {
	// LD H, B
	cpu.h = cpu.b;
}

static void cpu_instr_0x61(int *cycles) {
	// LD H, C
	cpu.h = cpu.c;
}

static void cpu_instr_0x62(int *cycles) {
	// LD H, D
	cpu.h = cpu.d;
}

static void cpu_instr_0x63(int *cycles) {
	// LD H, E
	cpu.h = cpu.e;
}

static void cpu_instr_0x64(int *cycles) {
	// LD H, H
	cpu.h = cpu.h;
}

static void cpu_instr_0x65(int *cycles) {
	// LD H, L
	cpu.h = cpu.l;
}

static void cpu_instr_0x66(int *cycles) {
	// LD H, (HL)
	cpu.h = read_byte(cpu.hl);
}

static void cpu_instr_0x67(int *cycles) {
	// LD H, A
	cpu.h = cpu.a;
}

static void cpu_instr_0x68(int *cycles) {
	// LD L, B
	cpu.l = cpu.b;
}

static void cpu_instr_0x69(int *cycles) {
	// LD L, C
	cpu.l = cpu.c;
}

static void cpu_instr_0x6a(int *cycles) {
	// LD L, D
	cpu.l = cpu.d;
}

static void cpu_instr_0x6b(int *cycles) {
	// LD L, E
	cpu.l = cpu.e;
}

static void cpu_instr_0x6c(int *cycles) {
	// LD L, H
	cpu.l = cpu.h;
}

static void cpu_instr_0x6d(int *cycles) {
	// LD L, L
	cpu.l = cpu.l;
}

static void cpu_instr_0x6e(int *cycles) {
	// LD L, (HL)
	cpu.l = read_byte(cpu.hl);
}

static void cpu_instr_0x6f(int *cycles) {
	// LD L, A
	cpu.l = cpu.a;
}

static void cpu_instr_0x70(int *cycles) {
	// LD (HL), B
	write_byte(cpu.hl, cpu.b);
}

static void cpu_instr_0x71(int *cycles) {
	// LD (HL), C
	write_byte(cpu.hl, cpu.c);
}

static void cpu_instr_0x72(int *cycles) {
	// LD (HL), D
	write_byte(cpu.hl, cpu.d);
}

static void cpu_instr_0x73(int *cycles) {
	// LD (HL), E
	write_byte(cpu.hl, cpu.e);
}

static void cpu_instr_0x74(int *cycles) {
	// LD (HL), H
	write_byte(cpu.hl, cpu.h);
}

static void cpu_instr_0x75(int *cycles) {
	// LD (HL), L
	write_byte(cpu.hl, cpu.l);
}

static void cpu_instr_0x76(int *cycles) {
	// HALT
}

static void cpu_instr_0x77(int *cycles) {
	// LD (HL), A
	write_byte(cpu.hl, cpu.a);
}

static void cpu_instr_0x78(int *cycles) {
	// LD A, B
	cpu.a = cpu.b;
}

static void cpu_instr_0x79(int *cycles) {
	// LD A, C
	cpu.a = cpu.c;
}

static void cpu_instr_0x7a(int *cycles) {
	// LD A, D
	cpu.a = cpu.d;
}

static void cpu_instr_0x7b(int *cycles) {
	// LD A, E
	cpu.a = cpu.e;
}

static void cpu_instr_0x7c(int *cycles) {
	// LD A, H
	cpu.a = cpu.h;
}

static void cpu_instr_0x7d(int *cycles) {
	// LD A, L
	cpu.a = cpu.l;
}

static void cpu_instr_0x7e(int *cycles) {
	// LD A, (HL)
	cpu.a = read_byte(cpu.hl);
}

static void cpu_instr_0x7f(int *cycles) {
	// LD A, A
	cpu.a = cpu.a;
}

static void cpu_instr_0x80(int *cycles) {
	// ADD A, B
	cpu_opcode_add_a(cpu.b);
}

static void cpu_instr_0x81(int *cycles) {
	// ADD A, C
	cpu_opcode_add_a(cpu.c);
}

static void cpu_instr_0x82(int *cycles) {
	// ADD A, D
	cpu_opcode_add_a(cpu.d);
}

static void cpu_instr_0x83(int *cycles) {
	// ADD A, E
	cpu_opcode_add_a(cpu.e);
}

static void cpu_instr_0x84(int *cycles) {
	// ADD A, H
	cpu_opcode_add_a(cpu.h);
}

static void cpu_instr_0x85(int *cycles) {
	// ADD A, L
	cpu_opcode_add_a(cpu.l);
}

static void cpu_instr_0x86(int *cycles) {
	// ADD A, (HL)
	cpu_opcode_add_a_ptr_hl();
}

static void cpu_instr_0x87(int *cycles) {
	// ADD A, A
	cpu_opcode_add_a(cpu.a);
}

static void cpu_instr_0x88(int *cycles) {
	// ADC A, B
	cpu_opcode_adc_a(cpu.b);
}

static void cpu_instr_0x89(int *cycles) {
	// ADC A, C
	cpu_opcode_adc_a(cpu.c);
}

static void cpu_instr_0x8a(int *cycles) {
	// ADC A, D
	cpu_opcode_adc_a(cpu.d);
}

static void cpu_instr_0x8b(int *cycles) {
	// ADC A, E
	cpu_opcode_adc_a(cpu.e);
}

static void cpu_instr_0x8c(int *cycles) {
	// ADC A, H
	cpu_opcode_adc_a(cpu.h);
}

static void cpu_instr_0x8d(int *cycles) {
	// ADC A, L
	cpu_opcode_adc_a(cpu.l);
}

static void cpu_instr_0x8e(int *cycles) {
	// ADC A, (HL)
	cpu_opcode_adc_a_ptr_hl();
}

static void cpu_instr_0x8f(int *cycles) {
	// ADC A, A
	cpu_opcode_adc_a(cpu.a);
}

static void cpu_instr_0x90(int *cycles) {
	// SUB B
	cpu_opcode_sub_a(cpu.b);
}

static void cpu_instr_0x91(int *cycles) {
	// SUB C
	cpu_opcode_sub_a(cpu.c);
}

static void cpu_instr_0x92(int *cycles) {
	// SUB D
	cpu_opcode_sub_a(cpu.d);
}

static void cpu_instr_0x93(int *cycles) {
	// SUB E
	cpu_opcode_sub_a(cpu.e);
}

static void cpu_instr_0x94(int *cycles) {
	// SUB H
	cpu_opcode_sub_a(cpu.h);
}

static void cpu_instr_0x95(int *cycles) {
	// SUB L
	cpu_opcode_sub_a(cpu.l);
}

static void cpu_instr_0x96(int *cycles) {
	// SUB (HL)
	cpu_opcode_sub_a_ptr_hl();
}

static void cpu_instr_0x97(int *cycles) {
	// SUB A
	cpu_opcode_sub_a(cpu.a);
}

static void cpu_instr_0x98(int *cycles) {
	// SBC A, B
	cpu_opcode_sbc_a(cpu.b);
}

static void cpu_instr_0x99(int *cycles) {
	// SBC A, C
	cpu_opcode_sbc_a(cpu.c);
}

static void cpu_instr_0x9a(int *cycles) {
	// SBC A, D
	cpu_opcode_sbc_a(cpu.d);
}

static void cpu_instr_0x9b(int *cycles) {
	// SBC A, E
	cpu_opcode_sbc_a(cpu.e);
}

static void cpu_instr_0x9c(int *cycles) {
	// SBC A, H
	cpu_opcode_sbc_a(cpu.h);
}

static void cpu_instr_0x9d(int *cycles) {
	// SBC A, L
	cpu_opcode_sbc_a(cpu.l);
}

static void cpu_instr_0x9e(int *cycles) {
	// SBC A, (HL)
	cpu_opcode_sbc_a_ptr_hl();
}

static void cpu_instr_0x9f(int *cycles) {
	// SBC A, A
	cpu_opcode_sbc_a(cpu.a);
}

static void cpu_instr_0xa0(int *cycles) {
	// AND B
	cpu.a = cpu.a & cpu.b;
	SET_AND_FLAGS();
}

static void cpu_instr_0xa1(int *cycles) {
	// AND C
	cpu.a = cpu.a & cpu.c;
	SET_AND_FLAGS();
}

static void cpu_instr_0xa2(int *cycles) {
	// AND D
	cpu.a = cpu.a & cpu.d;
	SET_AND_FLAGS();
}

static void cpu_instr_0xa3(int *cycles) {
	// AND E
	cpu.a = cpu.a & cpu.e;
	SET_AND_FLAGS();
}

static void cpu_instr_0xa4(int *cycles) {
	// AND H
	cpu.a = cpu.a & cpu.h;
	SET_AND_FLAGS();
}

static void cpu_instr_0xa5(int *cycles) {
	// AND L
	cpu.a = cpu.a & cpu.l;
	SET_AND_FLAGS();
}

static void cpu_instr_0xa6(int *cycles) {
	// AND (HL)
	cpu.a &= read_byte(cpu.hl);
	SET_AND_FLAGS();
}

static void cpu_instr_0xa7(int *cycles) {
	// AND A
	cpu.a &= cpu.a;
	SET_AND_FLAGS();
}

static void cpu_instr_0xa8(int *cycles) {
	// XOR B
	cpu.a = cpu.a ^ cpu.b;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xa9(int *cycles) {
	// XOR C
	cpu.a = cpu.a ^ cpu.c;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xaa(int *cycles) {
	// XOR D
	cpu.a = cpu.a ^ cpu.d;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xab(int *cycles) {
	// XOR E
	cpu.a = cpu.a ^ cpu.e;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xac(int *cycles) {
	// XOR H
	cpu.a = cpu.a ^ cpu.h;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xad(int *cycles) {
	// XOR L
	cpu.a = cpu.a ^ cpu.l;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xae(int *cycles) {
	// XOR (HL)
	cpu.a ^= read_byte(cpu.hl);
	SET_xOR_FLAGS();
}

static void cpu_instr_0xaf(int *cycles) {
	// XOR A
	cpu.a ^= cpu.a;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xb0(int *cycles) {
	// OR B
	cpu.a = cpu.a | cpu.b;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xb1(int *cycles) {
	// OR C
	cpu.a = cpu.a | cpu.c;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xb2(int *cycles) {
	// OR D
	cpu.a = cpu.a | cpu.d;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xb3(int *cycles) {
	// OR E
	cpu.a = cpu.a | cpu.e;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xb4(int *cycles) {
	// OR H
	cpu.a = cpu.a | cpu.h;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xb5(int *cycles) {
	// OR L
	cpu.a = cpu.a | cpu.l;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xb6(int *cycles) {
	// OR (HL)
	cpu.a |= read_byte(cpu.hl);
	SET_xOR_FLAGS();
}

static void cpu_instr_0xb7(int *cycles) {
	// OR A
	cpu.a |= cpu.a;
	SET_xOR_FLAGS();
}

static void cpu_instr_0xb8(int *cycles) {
	// CP B
	cpu_opcode_cp_a(cpu.b);
}

static void cpu_instr_0xb9(int *cycles) {
	// CP C
	cpu_opcode_cp_a(cpu.c);
}

static void cpu_instr_0xba(int *cycles) {
	// CP D
	cpu_opcode_cp_a(cpu.d);
}

static void cpu_instr_0xbb(int *cycles) {
	// CP E
	cpu_opcode_cp_a(cpu.e);
}

static void cpu_instr_0xbc(int *cycles) {
	// CP H
	cpu_opcode_cp_a(cpu.h);
}

static void cpu_instr_0xbd(int *cycles) {
	// CP L
	cpu_opcode_cp_a(cpu.l);
}

static void cpu_instr_0xbe(int *cycles) {
	// CP (HL)
	cpu_opcode_cp_a_ptr_hl();
}

static void cpu_instr_0xbf(int *cycles) {
	// CP A
	cpu_opcode_cp_a(cpu.a);
}

static void cpu_instr_0xc0(int *cycles) {
	// RET NZ
	if (!GET_FLAG(Z)) {
		cpu.pc = stack_pop();
	}
}

static void cpu_instr_0xc1(int *cycles) {
	// POP BC
	cpu.bc = stack_pop();
}

static void cpu_instr_0xc2(int *cycles) {
	// JP NZ, a16
	if (!GET_FLAG(Z)) {
		cpu.pc = read_word(cpu.pc);
		*cycles += 4;
	}
	else {
		cpu.pc += 2;
	}
}

static void cpu_instr_0xc3(int *cycles) {
	// JP a16
	cpu.pc = read_word(cpu.pc);
}

static void cpu_instr_0xc4(int *cycles) {
	// CALL NZ, a16
	if (!GET_FLAG(Z)) {
		stack_push(cpu.pc + 2);
		cpu.pc = read_word(cpu.pc);
		*cycles += 12;
	}
	else {
		cpu.pc += 2;
	}
}

static void cpu_instr_0xc5(int *cycles) {
	// PUSH BC
	stack_push(cpu.bc);
}

static void cpu_instr_0xc6(int *cycles) {
	// ADD A, d8
	cpu_opcode_add_a_d8();
}

static void cpu_instr_0xc7(int *cycles) {
	// RST 00H
	cpu_opcode_rst(0x00);
}

static void cpu_instr_0xc8(int *cycles) {
	// RET Z
	if (GET_FLAG(Z)) {
		cpu.pc = stack_pop();
	}
}

static void cpu_instr_0xc9(int *cycles) {
	// RET
	cpu.pc = stack_pop();
}

static void cpu_instr_0xca(int *cycles) {
	// JP Z, a16
	if (GET_FLAG(Z)) {
		cpu.pc = read_word(cpu.pc);
		*cycles += 4;
	}
	else {
		cpu.pc += 2;
	}
}

static void cpu_instr_0xcb(int *cycles) {
	// PREFIX CB
	cpu_prefix_cb_handle(cycles);
}

static void cpu_instr_0xcc(int *cycles) {
	// CALL Z, a16
	if (GET_FLAG(Z)) {
		stack_push(cpu.pc + 2);
		cpu.pc = read_word(cpu.pc);
		*cycles += 12;
	}
	else {
		cpu.pc += 2;
	}
}

static void cpu_instr_0xcd(int *cycles) {
	// CALL a16
	stack_push(cpu.pc + 2);
	cpu.pc = read_word(cpu.pc);
}

static void cpu_instr_0xce(int *cycles) {
	// ADC A, d8
	cpu_opcode_adc_a_d8();
}

static void cpu_instr_0xcf(int *cycles) {
	// RST 08H
	cpu_opcode_rst(0x08);
}

static void cpu_instr_0xd0(int *cycles) {
	// RET NC
	if (!GET_FLAG(C)) {
		cpu.pc = stack_pop();
	}
}

static void cpu_instr_0xd1(int *cycles) {
	// POP DE
	cpu.de = stack_pop();
}

static void cpu_instr_0xd2(int *cycles) {
	// JP NC, a16
	if (!GET_FLAG(C)) {
		cpu.pc = read_word(cpu.pc);
		*cycles += 4;
	}
	else {
		cpu.pc += 2;
	}
}

static void cpu_instr_0xd3(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xd4(int *cycles) {
	// CALL NC, a16
	if (!GET_FLAG(C)) {
		stack_push(cpu.pc + 2);
		cpu.pc = read_word(cpu.pc);
		*cycles += 12;
	}
	else {
		cpu.pc += 2;
	}
}

static void cpu_instr_0xd5(int *cycles) {
	// PUSH DE
	stack_push(cpu.de);
}

static void cpu_instr_0xd6(int *cycles) {
	// SUB d8
	cpu_opcode_sub_a_ptr_d8();
}

static void cpu_instr_0xd7(int *cycles) {
	// RST 10H
	cpu_opcode_rst(0x10);
}

static void cpu_instr_0xd8(int *cycles) {
	// RET C
	if (GET_FLAG(C)) {
		cpu.pc = stack_pop();
	}
}

static void cpu_instr_0xd9(int *cycles) {
	// RETI
	cpu.pc  = stack_pop();
	cpu.ime = 1;
}

static void cpu_instr_0xda(int *cycles) {
	// JP C, a16
	if (GET_FLAG(C)) {
		cpu.pc = read_word(cpu.pc);
		*cycles += 4;
	}
	else {
		cpu.pc += 2;
	}
}

static void cpu_instr_0xdb(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xdc(int *cycles) {
	// CALL C, a16
	if (GET_FLAG(C)) {
		stack_push(cpu.pc + 2);
		cpu.pc = read_word(cpu.pc);
		*cycles += 12;
	}
	else {
		cpu.pc += 2;
	}
}

static void cpu_instr_0xdd(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xde(int *cycles) {
	// SBC A, d8
	cpu_opcode_sbc_a_ptr_d8();
}

static void cpu_instr_0xdf(int *cycles) {
	// RST 18H
	cpu_opcode_rst(0x18);
}

static void cpu_instr_0xe0(int *cycles) {
	// LDH (a8), A
	write_byte(0xFF00 + read_byte(cpu.pc++), cpu.a);
}

static void cpu_instr_0xe1(int *cycles) {
	// POP HL
	cpu.hl = stack_pop();
}

static void cpu_instr_0xe2(int *cycles) {
	// LD (C), A
	write_byte(0xFF00 + cpu.c, cpu.a);
}

static void cpu_instr_0xe3(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xe4(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xe5(int *cycles) {
	// PUSH HL
	stack_push(cpu.hl);
}

static void cpu_instr_0xe6(int *cycles) {
	// AND d8
	cpu.a &= read_byte(cpu.pc++);
	SET_AND_FLAGS();
}

static void cpu_instr_0xe7(int *cycles) {
	// RST 20H
	cpu_opcode_rst(0x20);
}

static void cpu_instr_0xe8(int *cycles) {
	// ADD SP, r8
	cpu_opcode_add_sp((int8_t) read_byte(cpu.pc++));
}

static void cpu_instr_0xe9(int *cycles) {
	// JP (HL)
	cpu.pc = cpu.hl;
}

static void cpu_instr_0xea(int *cycles) {
	// LD (a16), A
	write_byte(read_word(cpu.pc), cpu.a);
	cpu.pc += 2;
}

static void cpu_instr_0xeb(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xec(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xed(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xee(int *cycles) {
	// XOR d8
	cpu.a ^= read_byte(cpu.pc);
	SET_xOR_FLAGS();
	cpu.pc++;
}

static void cpu_instr_0xef(int *cycles) {
	// RST 28H
	cpu_opcode_rst(0x28);
}

static void cpu_instr_0xf0(int *cycles) {
	// LDH A, (a8)
	cpu.a = read_byte(read_byte(cpu.pc) + 0xFF00);
	cpu.pc++;
}

static void cpu_instr_0xf1(int *cycles) {
	// POP AF
	cpu.af = stack_pop();
	cpu.f &= ~0xF;
}

static void cpu_instr_0xf2(int *cycles) {
	// LD A, (C)
	cpu.a = read_byte(0xFF00 + cpu.c);
}

static void cpu_instr_0xf3(int *cycles) {
	// DI
	cpu.ime = 0;
}

static void cpu_instr_0xf4(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xf5(int *cycles) {
	// PUSH AF
	stack_push(cpu.af);
}

static void cpu_instr_0xf6(int *cycles) {
	// OR d8
	cpu.a |= read_byte(cpu.pc);
	SET_xOR_FLAGS();
	cpu.pc++;
}

static void cpu_instr_0xf7(int *cycles) {
	// RST 30H
	cpu_opcode_rst(0x30);
}

static void cpu_instr_0xf8(int *cycles) {
	// LD HL, SP+r8
	cpu_opcode_ld_hl_sp((int8_t) read_byte(cpu.pc++));
}

static void cpu_instr_0xf9(int *cycles) {
	// LD SP, HL
	cpu.sp = cpu.hl;
}

static void cpu_instr_0xfa(int *cycles) {
	// LD A, (a16)
	cpu.a = read_byte(read_word(cpu.pc));
	cpu.pc += 2;
}

static void cpu_instr_0xfb(int *cycles) {
	// EI
	cpu.ime = 1;
}

static void cpu_instr_0xfc(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xfd(int *cycles) {
	// TODO CHECKME
	println("CHECKME");
}

static void cpu_instr_0xfe(int *cycles) {
	// CP d8
	cpu_opcode_cp_a_ptr_d8();
}

static void cpu_instr_0xff(int *cycles) {
	// RST 38H
	cpu_opcode_rst(0x38);
}

static inline ALWAYS_INLINE uint8_t read_byte(uint16_t addr) {
	uint8_t val = 0;
	if (addr >= 0 && addr <= 0x7FFF) {
		val = rom_read(addr);
		if (addr >= 0 && addr <= 0x00FF && cpu.boot_rom_enabled) {
			val = boot_rom[addr];
		}
	}
	else if (addr >= 0x8000 && addr <= 0x9FFF) {
		val = gpu_read(addr%0x8000);
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF) {
		val = sram[addr%0xA000];
	}
	else if (addr >= 0xC000 && addr <= 0xDFFF) {
		val = iram[addr%0xC000];
	}
	else if (addr >= 0xE000 && addr <= 0xFDFF) {
		val = iram[addr%0xE000];
	}
	else if (addr >= 0xFE00 && addr <= 0xFE9F) {
		val = gpu_oam_read(addr % 0xFE00);
	}
	else if (addr >= 0xFEA0 && addr <= 0xFEFF) {
		// unusable memory
	}
	else if (addr >= 0xFF00 && addr <= 0xFF7F) {
		val = cpu_read_register(addr);
	}
	else if (addr >= 0xFF80 && addr <= 0xFFFE) {
		val = zeropage[addr%0xFF80];
	}
	else {
		val = cpu.interrupt_enable;
	}
	return val;
}

static inline ALWAYS_INLINE void write_byte(uint16_t addr, uint8_t val) {
	if (addr >= 0 && addr <= 0x7FFF) {
		rom_write(addr, val);
	}
	else if (addr >= 0x8000 && addr <= 0x9FFF) {
		gpu_write(addr%0x8000, val);
	}
	else if (addr >= 0xA000 && addr <= 0xBFFF) {
		sram[addr%0xA000] = val;
	}
	else if (addr >= 0xC000 && addr <= 0xF0FF) {
		iram[addr%0xC000] = val;
	}
	else if (addr >= 0xE000 && addr <= 0xFDFF) {
		iram[addr%0xE000] = val;
	}
	else if (addr >= 0xFE00 && addr <= 0xFE9F) {
		gpu_oam_write(addr % 0xFE00, val);
	}
	else if (addr >= 0xFEA0 && addr <= 0xFEFF) {
		// unusable memory
	}
	else if (addr >= 0xFF00 && addr <= 0xFF7F) {
		cpu_write_register(addr, val);
	}
	else if (addr >= 0xFF80 && addr <= 0xFFFE) {
		zeropage[addr%0xFF80] = val;
	}
	else {
		cpu.interrupt_enable = val;
	}
}

static inline ALWAYS_INLINE uint16_t read_word(uint16_t addr) {
	uint8_t lo = read_byte(addr);
	uint8_t hi = read_byte(addr + 1);
	return (hi<<8) | lo;
}

static inline ALWAYS_INLINE void write_word (uint16_t addr, uint16_t val) {
	uint8_t lo = val & 0xFF;
	uint8_t hi = (val>>8) & 0xFF;
	write_byte(addr, lo);
	write_byte(addr + 1, hi);
}

static inline ALWAYS_INLINE void stack_push (uint16_t val) {
	uint8_t lo = val & 0xFF;
	uint8_t hi = (val>>8) & 0xFF;
	cpu.sp--;
	write_byte(cpu.sp, hi);
	cpu.sp--;
	write_byte(cpu.sp, lo);
}

static inline ALWAYS_INLINE uint16_t stack_pop (void) {
	uint8_t lo = read_byte(cpu.sp);
	cpu.sp++;
	uint8_t hi = read_byte(cpu.sp);
	cpu.sp++;
	return (hi<<8) | lo;
}

static void cpu_print_mem (uint16_t begin, uint16_t end) {
	for (int i = begin, c = 0; i <= end; ++i) {
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

static void cpu_dump_state () {
	println("Current INSTRUCTION POS: 0x%04x", cpu.pc - 1);
	println("CPU:\n Flags:\tZ:%d N:%d H:%d C:%d", GET_FLAG(Z), GET_FLAG(N), GET_FLAG(H), GET_FLAG(C));
	println("\tPC:0x%04x SP:0x%04x", cpu.pc, cpu.sp);
	println("REGISTERS:\n A:0x%02x B:0x%02x C:0x%02x D:0x%02x E:0x%02x H:0x%02x L:0x%02x F:0x%02x", cpu.a
	        , cpu.b, cpu.c, cpu.d, cpu.e, cpu.h, cpu.l, cpu.f);
}

static uint8_t cpu_read_register (uint16_t addr) {
	switch (addr) {
	case 0xFF0F:
		return cpu.interrupt_flag;

	case 0xFF40:
	case 0xFF41:
	case 0xFF42:
	case 0xFF43:
	case 0xFF44:
	case 0xFF45:
	case 0xFF46:
	case 0xFF47:
	case 0xFF48:
	case 0xFF49:
	case 0xFF4A:
	case 0xFF4B:
		return gpu_read_reg(addr);

	case 0xFF50:
		return cpu.boot_rom_enabled ? 1 : 0;

	case 0xFF00:
		return joypad_read_reg();

	case 0xFF01:
		return serial_read();

	case 0xFF02:
		return 0xff;

	case 0xFF10 ... 0xFF26:
		return sound_read_reg(addr);

	case 0xFF27 ... 0xFF2F:
		return 0x00;

	case 0xFF30 ... 0xFF3F:
		return sound_read_wavetable(addr);

	default:
		return 0x00;
	}
}

static void cpu_write_register (uint16_t addr, uint8_t val) {
	switch (addr) {
	case 0xFF0F:
		cpu.interrupt_flag = val | 0xE0;
		break;

	case 0xFF40:
	case 0xFF41:
	case 0xFF42:
	case 0xFF43:
	case 0xFF44:
	case 0xFF45:
	case 0xFF46:
	case 0xFF47:
	case 0xFF48:
	case 0xFF49:
	case 0xFF4A:
	case 0xFF4B:
		gpu_write_reg(addr, val);
		break;

	case 0xFF50:
		if (val == 0x1) {
			println("Disabling boot rom!");
			cpu.boot_rom_enabled = 0;
		}
		break;

	case 0xFF00:
		joypad_write_reg(val);
		break;

	case 0xFF01:
		serial_write(val);
		break;

	case 0xFF02:
		serial_write_control(val);
		break;

	case 0xFF10 ... 0xFF26:
		sound_write_reg(addr, val);
		break;

	case 0xFF27 ... 0xFF2F:
		// not used
		break;

	case 0xFF30 ... 0xFF3F:
		sound_write_wavetable(addr, val);
		break;

	default:
		break;
	}
}

static enum registers map_register (uint8_t opcode) {
	enum registers reg_code = (opcode & 0xF)%8;
	return reg_code;
}

static void cpu_opcode_bit (enum registers reg, uint8_t bit) {
	switch (reg) {
	case REG_B:
		SET_BIT_FLAGS(bit, cpu.b);
		break;
	case REG_C:
		SET_BIT_FLAGS(bit, cpu.c);
		break;
	case REG_D:
		SET_BIT_FLAGS(bit, cpu.d);
		break;
	case REG_E:
		SET_BIT_FLAGS(bit, cpu.e);
		break;
	case REG_H:
		SET_BIT_FLAGS(bit, cpu.h);
		break;
	case REG_L:
		SET_BIT_FLAGS(bit, cpu.l);
		break;
	case REG_HL:
		SET_BIT_FLAGS(bit, read_byte(cpu.hl));
		break;
	case REG_A:
		SET_BIT_FLAGS(bit, cpu.a);
		break;

	}
}

static inline void cpu_opcode_set(enum registers reg, uint8_t bit) {
	switch (reg) {
	case REG_B:
		cpu.b |= (1<<bit);
		break;
	case REG_C:
		cpu.c |= (1<<bit);
		break;
	case REG_D:
		cpu.d |= (1<<bit);
		break;
	case REG_E:
		cpu.e |= (1<<bit);
		break;
	case REG_H:
		cpu.h |= (1<<bit);
		break;
	case REG_L:
		cpu.l |= (1<<bit);
		break;
	case REG_HL:
		write_byte(cpu.hl, read_byte(cpu.hl) | (1<<bit));
		break;
	case REG_A:
		cpu.a |= (1<<bit);
		break;

	}
}

static inline void cpu_opcode_res(enum registers reg, uint8_t bit) {
	switch (reg) {
	case REG_B:
		cpu.b &= ~(1<<bit);
		break;
	case REG_C:
		cpu.c &= ~(1<<bit);
		break;
	case REG_D:
		cpu.d &= ~(1<<bit);
		break;
	case REG_E:
		cpu.e &= ~(1<<bit);
		break;
	case REG_H:
		cpu.h &= ~(1<<bit);
		break;
	case REG_L:
		cpu.l &= ~(1<<bit);
		break;
	case REG_HL:
		write_byte(cpu.hl, read_byte(cpu.hl) & ~(1<<bit));
		break;
	case REG_A:
		cpu.a &= ~(1<<bit);
		break;

	}
}

static inline void cpu_opcode_swap(enum registers reg) {
	switch (reg) {
	case REG_B:
		cpu.b = ((cpu.b & 0x0F)<<4 | (cpu.b & 0xF0)>>4);
		SET_SWAP_FLAGS(cpu.b);
		break;
	case REG_C:
		cpu.c = ((cpu.c & 0x0F)<<4 | (cpu.c & 0xF0)>>4);
		SET_SWAP_FLAGS(cpu.c);
		break;
	case REG_D:
		cpu.d = ((cpu.d & 0x0F)<<4 | (cpu.d & 0xF0)>>4);
		SET_SWAP_FLAGS(cpu.d);
		break;
	case REG_E:
		cpu.e = ((cpu.e & 0x0F)<<4 | (cpu.e & 0xF0)>>4);
		SET_SWAP_FLAGS(cpu.e);
		break;
	case REG_H:
		cpu.h = ((cpu.h & 0x0F)<<4 | (cpu.h & 0xF0)>>4);
		SET_SWAP_FLAGS(cpu.h);
		break;
	case REG_L:
		cpu.l = ((cpu.l & 0x0F)<<4 | (cpu.l & 0xF0)>>4);
		SET_SWAP_FLAGS(cpu.l);
		break;
	case REG_HL:
		write_byte(cpu.hl, (read_byte(cpu.hl) & 0x0F)<<4 | (read_byte(cpu.hl) & 0xF0)>>4);
		SET_SWAP_FLAGS(read_byte(cpu.hl));
		break;
	case REG_A:
		cpu.a = ((cpu.a & 0x0F)<<4 | (cpu.a & 0xF0)>>4);
		SET_SWAP_FLAGS(cpu.a);
		break;

	}
}

// move instr impl here

static inline void cpu_opcode_rst (const uint8_t offset) {
	stack_push(cpu.pc);
	cpu.pc = 0x0000 + offset;
}

static inline void cpu_opcode_interrupt (const uint8_t offset) {
	stack_push(cpu.pc);
	cpu.pc  = 0x0000 + offset;
	cpu.ime = 0;
}

/* this function only rotate data */
static inline uint8_t cpu_opcode_rl (uint8_t data) {
	bool c_flag   = GET_FLAG(C);
	bool new_flag = data & 0x80;
	data <<= 1;
	data |= (!!c_flag<<0);
	cpu.f = SET_FLAGS(data == 0, 0, 0, new_flag);
	return data;
}

static inline void cpu_opcode_rla() {
	bool c_flag   = GET_FLAG(C);
	bool new_flag = cpu.a & 0x80;
	cpu.a <<= 1;
	cpu.a |= (!!c_flag<<0);
	cpu.f         = SET_FLAGS(0, 0, 0, new_flag);
}

/* this function only rotate data */
static inline uint8_t cpu_opcode_rr(uint8_t data) {
	bool c_flag   = GET_FLAG(C);
	bool new_flag = data & 0x01;
	data >>= 1;
	data |= (!!c_flag<<7);
	cpu.f = SET_FLAGS(data == 0, 0, 0, new_flag);
	return data;
}

static inline void cpu_opcode_rra() {
	bool c_flag   = GET_FLAG(C);
	bool new_flag = cpu.a & 0x01;
	cpu.a >>= 1;
	cpu.a |= (c_flag<<7);
	cpu.f         = SET_FLAGS(0, 0, 0, new_flag);
}

static inline uint8_t cpu_opcode_rlc(uint8_t value) {
	bool new_flag = value & 0x80;
	value <<= 1;
	value |= !!new_flag;
	cpu.f = SET_FLAGS(value == 0, 0, 0, new_flag);
	return value;
}

static inline uint8_t cpu_opcode_rrc(uint8_t value) {
	bool new_flag = value & 0x01;
	value >>= 1;
	value |= (!!new_flag<<7);
	cpu.f = SET_FLAGS(value == 0, 0, 0, new_flag);
	return value;
}

static inline void cpu_opcode_rlca () {
	bool new_flag = cpu.a & 0x80;
	cpu.a <<= 1;
	cpu.a |= !!new_flag;
	cpu.f         = SET_FLAGS(0, 0, 0, new_flag);
}

static inline uint8_t cpu_opcode_sla (uint8_t value) {
	bool new_flag = value & 0x80;
	value <<= 1;
	cpu.f = SET_FLAGS(value == 0, 0, 0, new_flag);
	return value;
}

static inline uint8_t cpu_opcode_srl (uint8_t value) {
	bool new_flag = value & 0x01;
	value >>= 1;
	cpu.f = SET_FLAGS(value == 0, 0, 0, new_flag);
	return value;
}

static inline uint8_t cpu_opcode_sra (uint8_t value) {
	bool lo_flag = value & 0x01;
	bool hi_flag = value & 0x80;
	value >>= 1;
	value |= (hi_flag<<7);
	cpu.f = SET_FLAGS(value == 0, 0, 0, lo_flag);
	return value;
}

static inline void cpu_opcode_rrca () {
	bool new_flag = cpu.a & 0x01;
	cpu.a >>= 1;
	cpu.a |= (!!new_flag<<7);
	cpu.f         = SET_FLAGS(0, 0, 0, new_flag);
}

static inline void cpu_opcode_rl_full (enum registers reg) {
	switch (reg) {
	case REG_B:
		cpu.b = cpu_opcode_rl(cpu.b);
		break;
	case REG_C:
		cpu.c = cpu_opcode_rl(cpu.c);
		break;
	case REG_D:
		cpu.d = cpu_opcode_rl(cpu.d);
		break;
	case REG_E:
		cpu.e = cpu_opcode_rl(cpu.e);
		break;
	case REG_H:
		cpu.h = cpu_opcode_rl(cpu.h);
		break;
	case REG_L:
		cpu.l = cpu_opcode_rl(cpu.l);
		break;
	case REG_HL:
		write_byte(cpu.hl, cpu_opcode_rl(read_byte(cpu.hl)));
		break;
	case REG_A:
		cpu.a = cpu_opcode_rl(cpu.a);
		break;
	}
}

static inline void cpu_opcode_rr_full(enum registers reg) {
	switch (reg) {
	case REG_B:
		cpu.b = cpu_opcode_rr(cpu.b);
		break;
	case REG_C:
		cpu.c = cpu_opcode_rr(cpu.c);
		break;
	case REG_D:
		cpu.d = cpu_opcode_rr(cpu.d);
		break;
	case REG_E:
		cpu.e = cpu_opcode_rr(cpu.e);
		break;
	case REG_H:
		cpu.h = cpu_opcode_rr(cpu.h);
		break;
	case REG_L:
		cpu.l = cpu_opcode_rr(cpu.l);
		break;
	case REG_HL:
		write_byte(cpu.hl, cpu_opcode_rr(read_byte(cpu.hl)));
		break;
	case REG_A:
		cpu.a = cpu_opcode_rr(cpu.a);
		break;
	}
}

static inline void cpu_opcode_rlc_full(enum registers reg) {
	switch (reg) {
	case REG_B:
		cpu.b = cpu_opcode_rlc(cpu.b);
		break;
	case REG_C:
		cpu.c = cpu_opcode_rlc(cpu.c);
		break;
	case REG_D:
		cpu.d = cpu_opcode_rlc(cpu.d);
		break;
	case REG_E:
		cpu.e = cpu_opcode_rlc(cpu.e);
		break;
	case REG_H:
		cpu.h = cpu_opcode_rlc(cpu.h);
		break;
	case REG_L:
		cpu.l = cpu_opcode_rlc(cpu.l);
		break;
	case REG_HL:
		write_byte(cpu.hl, cpu_opcode_rlc(read_byte(cpu.hl)));
		break;
	case REG_A:
		cpu.a = cpu_opcode_rlc(cpu.a);
		break;
	}
}

static inline void cpu_opcode_rrc_full(enum registers reg) {
	switch (reg) {
	case REG_B:
		cpu.b = cpu_opcode_rrc(cpu.b);
		break;
	case REG_C:
		cpu.c = cpu_opcode_rrc(cpu.c);
		break;
	case REG_D:
		cpu.d = cpu_opcode_rrc(cpu.d);
		break;
	case REG_E:
		cpu.e = cpu_opcode_rrc(cpu.e);
		break;
	case REG_H:
		cpu.h = cpu_opcode_rrc(cpu.h);
		break;
	case REG_L:
		cpu.l = cpu_opcode_rrc(cpu.l);
		break;
	case REG_HL:
		write_byte(cpu.hl, cpu_opcode_rrc(read_byte(cpu.hl)));
		break;
	case REG_A:
		cpu.a = cpu_opcode_rrc(cpu.a);
		break;
	}
}

static inline void cpu_opcode_sla_full (enum registers reg) {
	switch (reg) {
	case REG_B:
		cpu.b = cpu_opcode_sla(cpu.b);
		break;
	case REG_C:
		cpu.c = cpu_opcode_sla(cpu.c);
		break;
	case REG_D:
		cpu.d = cpu_opcode_sla(cpu.d);
		break;
	case REG_E:
		cpu.e = cpu_opcode_sla(cpu.e);
		break;
	case REG_H:
		cpu.h = cpu_opcode_sla(cpu.h);
		break;
	case REG_L:
		cpu.l = cpu_opcode_sla(cpu.l);
		break;
	case REG_HL:
		write_byte(cpu.hl, cpu_opcode_sla(read_byte(cpu.hl)));
		break;
	case REG_A:
		cpu.a = cpu_opcode_sla(cpu.a);
		break;
	}
}

static inline void cpu_opcode_srl_full (enum registers reg) {
	switch (reg) {
	case REG_B:
		cpu.b = cpu_opcode_srl(cpu.b);
		break;
	case REG_C:
		cpu.c = cpu_opcode_srl(cpu.c);
		break;
	case REG_D:
		cpu.d = cpu_opcode_srl(cpu.d);
		break;
	case REG_E:
		cpu.e = cpu_opcode_srl(cpu.e);
		break;
	case REG_H:
		cpu.h = cpu_opcode_srl(cpu.h);
		break;
	case REG_L:
		cpu.l = cpu_opcode_srl(cpu.l);
		break;
	case REG_HL:
		write_byte(cpu.hl, cpu_opcode_srl(read_byte(cpu.hl)));
		break;
	case REG_A:
		cpu.a = cpu_opcode_srl(cpu.a);
		break;
	}
}

static inline void cpu_opcode_sra_full (enum registers reg) {
	switch (reg) {
	case REG_B:
		cpu.b = cpu_opcode_sra(cpu.b);
		break;
	case REG_C:
		cpu.c = cpu_opcode_sra(cpu.c);
		break;
	case REG_D:
		cpu.d = cpu_opcode_sra(cpu.d);
		break;
	case REG_E:
		cpu.e = cpu_opcode_sra(cpu.e);
		break;
	case REG_H:
		cpu.h = cpu_opcode_sra(cpu.h);
		break;
	case REG_L:
		cpu.l = cpu_opcode_sra(cpu.l);
		break;
	case REG_HL:
		write_byte(cpu.hl, cpu_opcode_sra(read_byte(cpu.hl)));
		break;
	case REG_A:
		cpu.a = cpu_opcode_sra(cpu.a);
		break;
	}
}

static void cpu_prefix_cb_handle (int *cycles) {
	uint8_t instruction = read_byte(cpu.pc++);
	*cycles += cycles_0xCB_opcodes[instruction];
	enum registers reg = map_register(instruction);
	// now we should mask register bits in instruction
	instruction &= 0xF8;
	//println("Instruction is 0x%02x, cycles %d, affected reg %d", instruction, *cycles, reg);

	switch (instruction) {
	case 0x00: // RLC
		cpu_opcode_rlc_full(reg);
		break;
	case 0x08: // RRC
		cpu_opcode_rrc_full(reg);
		break;
	case 0x10: // RL
		cpu_opcode_rl_full(reg);
		break;
	case 0x18: // RR
		cpu_opcode_rr_full(reg);
		break;
	case 0x20: // SLA
		cpu_opcode_sla_full(reg);
		break;
	case 0x28: // SRA
		cpu_opcode_sra_full(reg);
		break;
	case 0x30: // SWAP
		cpu_opcode_swap(reg);
		break;
	case 0x38: // SRL
		cpu_opcode_srl_full(reg);
		break;
	case 0x40: // BIT 0
		cpu_opcode_bit(reg, 0);
		break;
	case 0x48: // BIT 1
		cpu_opcode_bit(reg, 1);
		break;
	case 0x50: // BIT 2
		cpu_opcode_bit(reg, 2);
		break;
	case 0x58: // BIT 3
		cpu_opcode_bit(reg, 3);
		break;
	case 0x60: // BIT 4
		cpu_opcode_bit(reg, 4);
		break;
	case 0x68: // BIT 5
		cpu_opcode_bit(reg, 5);
		break;
	case 0x70: // BIT 6
		cpu_opcode_bit(reg, 6);
		break;
	case 0x78: // BIT 7
		cpu_opcode_bit(reg, 7);
		break;
	case 0x80: // RES 0
		cpu_opcode_res(reg, 0);
		break;
	case 0x88: // RES 1
		cpu_opcode_res(reg, 1);
		break;
	case 0x90: // RES 2
		cpu_opcode_res(reg, 2);
		break;
	case 0x98: // RES 3
		cpu_opcode_res(reg, 3);
		break;
	case 0xA0: // RES 4
		cpu_opcode_res(reg, 4);
		break;
	case 0xA8: // RES 5
		cpu_opcode_res(reg, 5);
		break;
	case 0xB0: // RES 6
		cpu_opcode_res(reg, 6);
		break;
	case 0xB8: // RES 7
		cpu_opcode_res(reg, 7);
		break;
	case 0xC0: // SET 0
		cpu_opcode_set(reg, 0);
		break;
	case 0xC8: // SET 1
		cpu_opcode_set(reg, 1);
		break;
	case 0xD0: // SET 2
		cpu_opcode_set(reg, 2);
		break;
	case 0xD8: // SET 3
		cpu_opcode_set(reg, 3);
		break;
	case 0xE0: // SET 4
		cpu_opcode_set(reg, 4);
		break;
	case 0xE8: // SET 5
		cpu_opcode_set(reg, 5);
		break;
	case 0xF0: // SET 6
		cpu_opcode_set(reg, 6);
		break;
	case 0xF8: // SET 7
		cpu_opcode_set(reg, 7);
		break;
	default:
		break;
	}
}

// TODO: TOTALLY BROKEN, FIX DAA INSTRUCTION
static inline void cpu_opcode_daa() {
	bool     n_flag     = GET_FLAG(N);
	uint16_t correction = GET_FLAG(C) ? 0x60 : 0x00;

	if ((!n_flag && (cpu.a & 0x0F) > 0x09) || GET_FLAG(H)) {
		correction |= 0x06;
	}

	if ((!n_flag && (cpu.a & 0xF0) > 0x90) || GET_FLAG(C)) {
		correction |= 0x60;
	}

	if (n_flag) {
		cpu.a = cpu.a - correction;
	}
	else {
		cpu.a = cpu.a + correction;
	}

	cpu.f = SET_FLAGS(cpu.a == 0, n_flag, 0, (((correction<<2) & 0x100) != 0));
}

static inline void cpu_opcode_add_a(uint8_t value) {
	bool h_flag = ((cpu.a & 0x0F) + (value & 0x0F)) > 0xf;
	bool c_flag = (cpu.a + value) > 0xFF;
	cpu.a = cpu.a + value;
	cpu.f = SET_FLAGS(cpu.a == 0, 0, h_flag, c_flag);
}

static inline void cpu_opcode_add_a_ptr_hl() {
	cpu_opcode_add_a(read_byte(cpu.hl));
}

static inline void cpu_opcode_add_a_d8() {
	cpu_opcode_add_a(read_byte(cpu.pc++));
}

static inline void cpu_opcode_adc_a(uint8_t value) {
	uint8_t c_flag_now = GET_FLAG(C);
	bool    h_flag     = ((cpu.a & 0x0F) + (value & 0x0F) + c_flag_now) > 0xf;
	bool    c_flag     = ((uint16_t) cpu.a + value + c_flag_now) > 0xFF;
	cpu.a = cpu.a + value + c_flag_now;
	cpu.f = SET_FLAGS(cpu.a == 0, 0, h_flag, c_flag);
}

static inline void cpu_opcode_adc_a_ptr_hl() {
	cpu_opcode_adc_a(read_byte(cpu.hl));
}

static inline void cpu_opcode_adc_a_d8() {
	cpu_opcode_adc_a(read_byte(cpu.pc++));
}

static inline void cpu_opcode_sub_a(uint8_t value) {
	int  res    = cpu.a - value;
	bool h_flag = ((cpu.a & 0x0F) - (value & 0x0F)) < 0;
	cpu.a = res;
	cpu.f = SET_FLAGS(cpu.a == 0, 1, h_flag, res < 0);
}

static inline void cpu_opcode_sub_a_ptr_hl() {
	cpu_opcode_sub_a(read_byte(cpu.hl));
}

static inline void cpu_opcode_sub_a_ptr_d8() {
	cpu_opcode_sub_a(read_byte(cpu.pc++));
}

static inline void cpu_opcode_sbc_a(uint8_t value) {
	bool c_flag_now = GET_FLAG(C);
	int  res        = (uint8_t) cpu.a - (uint8_t) value - (uint8_t) c_flag_now;
	bool h_flag     = ((cpu.a & 0x0F) - (value & 0x0F) - c_flag_now) < 0;
	cpu.a = (uint8_t) res;
	cpu.f = SET_FLAGS(cpu.a == 0, 1, h_flag, res < 0);
}

static inline void cpu_opcode_sbc_a_ptr_hl() {
	cpu_opcode_sbc_a(read_byte(cpu.hl));
}

static inline void cpu_opcode_sbc_a_ptr_d8() {
	cpu_opcode_sbc_a(read_byte(cpu.pc));
	cpu.pc++;
}

static inline void cpu_opcode_cp_a(uint8_t value) {
	int  res    = cpu.a - value;
	bool h_flag = ((cpu.a & 0x0F) - (value & 0x0F)) < 0;
	bool z_flag = (res == 0);
	cpu.f = SET_FLAGS(z_flag, 1, h_flag, res < 0);
}

static inline void cpu_opcode_cp_a_ptr_hl() {
	cpu_opcode_cp_a(read_byte(cpu.hl));
}

static inline void cpu_opcode_cp_a_ptr_d8() {
	cpu_opcode_cp_a(read_byte(cpu.pc++));
}

static inline void cpu_opcode_add_hl(uint16_t value) {
	uint32_t res    = cpu.hl + value;
	bool     h_flag = ((cpu.hl & 0xfff) + (value & 0xfff) > 0xfff);
	uint8_t  c_flag = ((res & 0x10000) != 0);
	bool     z_flag = GET_FLAG(Z);
	cpu.hl = (uint16_t) res;
	cpu.f  = SET_FLAGS(z_flag, 0, h_flag, c_flag);
}

static inline void cpu_opcode_add_sp(int8_t value) {
	int     res    = cpu.sp + value;
	bool    h_flag = (((cpu.sp ^ value ^ (res & 0xFFFF)) & 0x10) == 0x10);
	uint8_t c_flag = (((cpu.sp ^ value ^ (res & 0xFFFF)) & 0x100) == 0x100);
	cpu.sp = (uint16_t) res;
	cpu.f  = SET_FLAGS(0, 0, h_flag, c_flag);
}

static inline void cpu_opcode_ccf() {
	cpu.f = SET_FLAGS(GET_FLAG(Z), 0, 0, !GET_FLAG(C));
}

static inline void cpu_opcode_scf() {
	cpu.f = SET_FLAGS(GET_FLAG(Z), 0, 0, true);
}

static inline void cpu_opcode_cpl() {
	cpu.a = ~cpu.a;
	cpu.f = SET_FLAGS(GET_FLAG(Z), 1, 1, GET_FLAG(C));
}

static inline void cpu_opcode_ld_hl_sp(int8_t value) {
	int  res    = cpu.sp + value;
	bool h_flag = ((value ^ cpu.sp ^ (res & 0xFFFF)) & 0x10) == 0x10;
	bool c_flag = ((value ^ cpu.sp ^ (res & 0xFFFF)) & 0x100) == 0x100;
	cpu.f  = SET_FLAGS(0, 0, h_flag, c_flag);
	cpu.hl = (uint16_t) res;
}
