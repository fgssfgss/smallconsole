#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define ALWAYS_INLINE __attribute__((always_inline))

void init_common();

void shutdown_common();

void println(const char *message, ...);

void printl(const char *message, ...);

#endif /* _COMMON_H_ */