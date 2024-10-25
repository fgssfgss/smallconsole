#ifndef _TIMER_H_
#define _TIMER_H_

#include "common.h"

uint8_t timer_read_reg (uint16_t addr);
void timer_write_reg (uint16_t addr, uint8_t val);
void timer_step (int cycles);

#endif /* _TIMER_H_ */
