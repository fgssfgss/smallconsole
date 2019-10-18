#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void init_common();

void shutdown_common();

void printl(const char *message, ...);

#endif /* _COMMON_H_ */