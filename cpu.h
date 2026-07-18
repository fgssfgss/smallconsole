#ifndef _CPU_H_
#define _CPU_H_

void cpu_init ();

int cpu_step ();

void cpu_request_interrupt (int bit);

uint8_t cpu_get_dma (uint8_t start_addr, uint8_t index);

#ifdef DEBUG_BUILD
// returns cycles spent halted vs actively executing since the last call,
// then resets both counters (debug/diagnostic use only)
void cpu_get_debug_stats (uint32_t *halted_cycles, uint32_t *active_cycles);
#endif

#endif /* _CPU_H_ */