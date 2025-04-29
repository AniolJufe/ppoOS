#include "limine_libc.h"

// Syscall wrappers
void exit(int status) {
    _syscall(SYS_EXIT, status, 0, 0, 0, 0);
    
    // If we get here, the exit syscall didn't terminate execution
    // Use an infinite loop to make sure we never return
    for (;;) {
        // Try the syscall again with different error code
        _syscall(SYS_EXIT, 0xDEAD, 0, 0, 0, 0);
        // Add a halt hint for the CPU (might work on some systems)
        __asm__ volatile("hlt");
    }
}

int write(int fd, const void *buf, size_t count) {
    return _syscall(SYS_WRITE, fd, (uint64_t)buf, count, 0, 0);
}

int read(int fd, void *buf, size_t count) {
    return _syscall(SYS_READ, fd, (uint64_t)buf, count, 0, 0);
}

int open(const char *pathname, int flags) {
    return _syscall(SYS_OPEN, (uint64_t)pathname, flags, 0, 0, 0);
}

int close(int fd) {
    return _syscall(SYS_CLOSE, fd, 0, 0, 0, 0);
}

// String functions
size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

void *memcpy(void *dest, const void *src, size_t n) {
    char *d = dest;
    const char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

// Printf function (simplified)
int printf(const char *format, ...) {
    // This is a very limited implementation - just handles %s, %d, and %c
    char buffer[1024];
    size_t pos = 0;
    
    // Parse the format string
    for (size_t i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%' && format[i + 1] != '\0') {
            i++;
            switch (format[i]) {
                case 'd': {
                    // Get the argument (this is a hack - in a real implementation we'd use va_args)
                    int value = *(int*)((uint64_t)&format + sizeof(char*) + sizeof(int));
                    
                    // Convert to string
                    char num_buf[20];
                    int num_pos = 0;
                    int temp = value;
                    
                    // Handle negative
                    if (temp < 0) {
                        buffer[pos++] = '-';
                        temp = -temp;
                    }
                    
                    // Handle zero
                    if (temp == 0) {
                        buffer[pos++] = '0';
                    } else {
                        // Convert to digits (in reverse)
                        while (temp > 0) {
                            num_buf[num_pos++] = '0' + (temp % 10);
                            temp /= 10;
                        }
                        
                        // Add to buffer in correct order
                        while (num_pos > 0) {
                            buffer[pos++] = num_buf[--num_pos];
                        }
                    }
                    break;
                }
                case 's': {
                    // Get the argument (this is a hack - in a real implementation we'd use va_args)
                    char *str = *(char**)((uint64_t)&format + sizeof(char*));
                    
                    // Copy the string
                    while (*str) {
                        buffer[pos++] = *str++;
                    }
                    break;
                }
                case 'c': {
                    // Get the argument (this is a hack - in a real implementation we'd use va_args)
                    char c = *(char*)((uint64_t)&format + sizeof(char*) + sizeof(int));
                    
                    // Add to buffer
                    buffer[pos++] = c;
                    break;
                }
                case '%':
                    buffer[pos++] = '%';
                    break;
                default:
                    buffer[pos++] = '%';
                    buffer[pos++] = format[i];
                    break;
            }
        } else {
            buffer[pos++] = format[i];
        }
        
        // Make sure we don't overflow
        if (pos >= sizeof(buffer) - 1) {
            break;
        }
    }
    
    // Null terminate
    buffer[pos] = '\0';
    
    // Write to stdout
    return write(STDOUT, buffer, pos);
}

int puts(const char *s) {
    // Get string length
    size_t len = strlen(s);
    
    // Write the string
    int res = write(STDOUT, s, len);
    
    // Write a newline
    if (res >= 0) {
        write(STDOUT, "\n", 1);
        return res + 1;
    }
    
    return res;
}
