#ifndef _CPU_H_
#define _CPU_H_

void init_cpu ();

void cpu_load_rom (const char *rom_filename);

void cpu_tick ();

void cpu_request_interrupt (int bit);

uint8_t cpu_get_dma (uint8_t start_addr, uint8_t index);

#endif /* _CPU_H_ */