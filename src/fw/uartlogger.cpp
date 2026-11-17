// Ф0.2 bench: fake-08 logger to the MMIO console (make LOG=1), else test/nooplogger.cpp
#include <stdarg.h>
#include <stdio.h>
#include "logger.h"

void Logger_Initialize(const char *) {}
void Logger_Exit() {}
void Logger_LogOutput(const char *func, size_t line, const char *format, ...) {
    va_list ap; va_start(ap, format);
    printf("LOG %s:%u ", func, (unsigned)line); vprintf(format, ap);
    va_end(ap);
}
void Logger_Write(const char *format, ...) {
    va_list ap; va_start(ap, format);
    printf("LOG "); vprintf(format, ap);
    va_end(ap);
}
void Logger_WriteUnformatted(const char *message) { printf("LOG %s", message); }
