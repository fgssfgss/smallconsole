#include "common.h"
#include <stdarg.h>

static FILE *log = NULL;

void init_common() {
	// FIXME: remove this stub
	log = stdout;
}

void shutdown_common() {

}

// just a log routine
void println(const char *message, ...) {
	va_list arg;

	va_start(arg, message);
	vfprintf(log, message, arg);
	va_end(arg);

	fprintf(log, "\r\n");
	fflush(log);
}

void printl(const char *message, ...) {
    va_list arg;

    va_start(arg, message);
    vfprintf(log, message, arg);
    va_end(arg);

    fflush(log);
}