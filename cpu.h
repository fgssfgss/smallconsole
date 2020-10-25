#ifndef _CPU_H_
#define _CPU_H_

void init_cpu();

void cpu_load_rom(const char *rom_filename);

void cpu_run();

void cpu_request_interrupt(int bit);

#endif /* _CPU_H_ */