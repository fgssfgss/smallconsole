#ifndef _CPU_H_
#define _CPU_H_

void cpu_init ();

int cpu_step ();

void cpu_request_interrupt (int bit);

uint8_t cpu_get_dma (uint8_t start_addr, uint8_t index);

#endif /* _CPU_H_ */