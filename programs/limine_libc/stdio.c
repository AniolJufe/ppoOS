#include "stdio.h"
#include "syscall.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// Printf function (simplified)
int printf(const char *format, ...) {
    char buf[512];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    write(STDOUT, buf, len);
    return len;
}

int puts(const char *s) {
    int len = 0;
    while (s[len]) len++;
    write(STDOUT, s, len);
    write(STDOUT, "\n", 1);
    return len + 1;
}

// vsnprintf: minimal implementation for printf
int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    // For brevity, only implement %s, %d, %x, %c
    size_t i = 0;
    for (; *fmt && i + 1 < size; fmt++) {
        if (*fmt != '%') {
            buf[i++] = *fmt;
            continue;
        }
        fmt++;
        if (*fmt == 's') {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            while (*s && i + 1 < size) buf[i++] = *s++;
        } else if (*fmt == 'd') {
            int num = va_arg(args, int);
            char tmp[32];
            int neg = (num < 0);
            size_t j = 0;
            if (neg) num = -num;
            do { tmp[j++] = '0' + (num % 10); num /= 10; } while (num && j < sizeof(tmp));
            if (neg && i + 1 < size) buf[i++] = '-';
            while (j-- && i + 1 < size) buf[i++] = tmp[j];
        } else if (*fmt == 'x') {
            unsigned num = va_arg(args, unsigned);
            char tmp[32];
            size_t j = 0;
            do { tmp[j++] = "0123456789abcdef"[num % 16]; num /= 16; } while (num && j < sizeof(tmp));
            while (j-- && i + 1 < size) buf[i++] = tmp[j];
        } else if (*fmt == 'c') {
            buf[i++] = (char)va_arg(args, int);
        } else {
            buf[i++] = '%';
            if (*fmt) buf[i++] = *fmt;
        }
    }
    buf[i] = 0;
    return i;
}
