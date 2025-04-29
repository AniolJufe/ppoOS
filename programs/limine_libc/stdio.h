#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>
#include <stddef.h>

// Standard file descriptors
#define STDIN  0
#define STDOUT 1
#define STDERR 2

int printf(const char *format, ...);
int puts(const char *s);
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args);

#endif // STDIO_H
