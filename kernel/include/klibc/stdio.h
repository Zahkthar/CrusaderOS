#ifndef KLIBC_STDIO_H_INCLUDED
#define KLIBC_STDIO_H_INCLUDED

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// +--------------------+
// | PRINTING FUNCTIONS |
// +--------------------+

int vsnprintf(char *buffer, size_t size, const char *format, va_list args);
int snprintf(char *buffer, size_t size, const char *format, ...);
int sprintf(char *buffer, const char *format, ...);

void kprintf(const char *format, ...);

#endif // KLIBC_STDIO_H_INCLUDED
