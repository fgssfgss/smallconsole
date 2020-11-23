#ifndef _GPU_H
#define _GPU_H

#include "common.h"

uint8_t gpu_read(uint16_t addr);
void gpu_write(uint16_t addr, uint8_t val);
void gpu_write_reg(uint16_t addr, uint8_t val);
uint8_t gpu_read_reg(uint16_t addr);
void gpu_oam_write(uint16_t addr, uint8_t val);
uint8_t gpu_oam_read(uint16_t addr);
void gpu_step(int cycles);
void gpu_init(void);

#endif //_GPU_H
