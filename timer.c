#include "timer.h"
#include "cpu.h"

static uint16_t timer_divider_increase = 0;
static uint8_t timer_divider = 0;

static uint32_t timer_counter_increase = 0;
static uint8_t timer_counter = 0;

static uint8_t timer_ctrl = 0xF8;
static uint8_t timer_modulo = 0;

void timer_step (int cycles) {
    timer_divider_increase += cycles;
    if (timer_divider_increase >= 256) {
        timer_divider_increase -= 256;
        timer_divider++;
    }

    if ((timer_ctrl >> 2) & 0x1) {
        int timer_increase_by = 0;

        switch((timer_ctrl & 0x3)) {
        case 0:
            timer_increase_by = 256;
            break;
        case 1:
            timer_increase_by = 16384;
            break;
        case 2:
            timer_increase_by = 4096;
            break;
        case 3:
            timer_increase_by = 1024;
            break;
        }

        timer_counter_increase += (timer_increase_by * cycles);
        if (timer_counter_increase >= 262144) {
            timer_counter_increase -= 262144;
            timer_counter++;

            if (timer_counter == 0) {
                timer_counter = timer_modulo;

                cpu_request_interrupt(2);
            }
        }
    }
}

void timer_write_reg (uint16_t addr, uint8_t val) {
    printf("WRITING TO TIMER 0x%04x val %d\n", addr, val);
    switch (addr) {
    case 0xff04:
        timer_divider = 0;
        break;
    case 0xff05:
        timer_counter = 0;
        break;
    case 0xff06:
        timer_modulo = val;
        break;
    case 0xff07:
        timer_ctrl = val;
        break;
    }
}

uint8_t timer_read_reg (uint16_t addr) {
    printf("READING FROM TIMER REG 0x%04x\n", addr);
    switch (addr) {
    case 0xff04:
        return timer_divider;
    case 0xff05:
        return timer_counter;
    case 0xff06:
        return timer_modulo;
    case 0xff07:
        return timer_ctrl;
    }
}
