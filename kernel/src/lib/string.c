#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declarations for helper functions used in strtok_r
size_t strspn(const char *s, const char *accept);
char *strpbrk(const char *s, const char *accept);

void *memcpy(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    for (size_t i = 0; i < n; i++) {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t n) {
    uint8_t *p = (uint8_t *)s;

    for (size_t i = 0; i < n; i++) {
        p[i] = (uint8_t)c;
    }

    return s;
}
    
void *memmove(void *dest, const void *src, size_t n) {
    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (src > dest) {
        for (size_t i = 0; i < n; i++) {
            pdest[i] = psrc[i];
        }
    } else if (src < dest) {
        for (size_t i = n; i > 0; i--) {
            pdest[i-1] = psrc[i-1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}
    
int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) s1++, s2++;
    return (unsigned char)*s1 - (unsigned char)*s2;
}
    
int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 != '\0' && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0; // First n characters matched
    }
    // If loop terminated early, return difference at point of mismatch or null terminator
    return (unsigned char)*s1 - (unsigned char)*s2;
}
    
char *strcpy(char *dest, const char *src) {
    char *p = dest;
    while (*src) *p++ = *src++;
    *p = '\0'; // Ensure null termination
    return dest;
}
    
char *strncpy(char *dest, const char *src, size_t n) {
    char *p = dest;
    size_t i = 0;
    while (i < n && *src) {
        *p++ = *src++;
        i++;
    }
    // Pad with nulls if n > strlen(src)
    while (i < n) {
        *p++ = '\0';
        i++;
    }
    // Note: strncpy doesn't guarantee null termination if src is longer than n
    // If null termination is always required, add it manually after the loop if needed.
    return dest;
}
    
char *strchr(const char *s, int c) {
    while (*s != (char)c) {
        if (*s == '\0') {
            return NULL; // Not found
        }
        s++;
    }
    return (char *)s; // Found
}
    
char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s != '\0') {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    // Check the null terminator as well if c is '\0'
    if (c == '\0') {
        return (char *)s;
    }
    return (char *)last;
}

// Minimal implementation of uitoa (unsigned integer to ASCII)
char* uitoa(unsigned int value, char* buffer, int base) {
    if (base < 2 || base > 16) {
        *buffer = '\0';
        return buffer; // Invalid base
    }

    char* ptr = buffer;
    char* ptr1 = buffer;
    char tmp_char;
    unsigned int tmp_value;

    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return buffer;
    }

    while (value != 0) {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdef"[tmp_value % base];
    }

    // Apply negative sign (not needed for unsigned, kept for structure)
    // if (sign) *ptr++ = '-';
    *ptr-- = '\0'; // Null terminate

    // Reverse the string
    while(ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr--= *ptr1;
        *ptr1++ = tmp_char;
    }
    return buffer;
}

// Minimal implementation of ultoa_hex (unsigned long to hex ASCII)
// Assumes buffer is large enough.
char* ultoa_hex(unsigned long value, char* buffer) {
    char* ptr = buffer;
    const char* hex_digits = "0123456789abcdef";
    int i = 0;
    bool leading_zero = true;

    if (value == 0) {
        *ptr++ = '0';
        *ptr = '\0';
        return buffer;
    }

    // Process 64 bits, 4 bits at a time (16 hex digits)
    for (i = 60; i >= 0; i -= 4) {
        unsigned long digit = (value >> i) & 0xF;
        if (digit != 0 || !leading_zero || i == 0) { // Ensure at least one digit if value is non-zero
            leading_zero = false;
            *ptr++ = hex_digits[digit];
        }
    }

    *ptr = '\0'; // Null terminate
    return buffer;
}

// Implementation of strcat (string concatenation)
char* strcat(char* dest, const char* src) {
    char* ptr = dest + strlen(dest); // Find the end of dest

    while (*src != '\0') {
        *ptr++ = *src++; // Copy src to the end of dest
    }

    *ptr = '\0'; // Null terminate the result
    return dest;
}


// Implementation of strncat (string concatenation with size limit)
char* strncat(char* dest, const char* src, size_t n) {
    char* ptr = dest + strlen(dest); // Find the end of dest
    size_t i = 0;

    while (*src != '\0' && i < n) {
        *ptr++ = *src++; // Copy src to the end of dest
        i++;
    }

    *ptr = '\0'; // Null terminate the result
    return dest;
}

// Implementation of strnlen (string length with size limit)
size_t strnlen(const char *s, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen && s[len]) {
        len++;
    }
    return len;
}

// Implementation of strtok_r (reentrant string tokenizer)
// Needed because standard strtok uses static state
char* strtok_r(char *str, const char *delim, char **saveptr) {
    char *token;

    if (str == NULL) {
        str = *saveptr;
    }

    // Skip leading delimiters using our helper function
    str += strspn(str, delim);
    if (*str == '\0') {
        *saveptr = str;
        return NULL;
    }

    // Find the end of the token using our helper function
    token = str;
    str = strpbrk(token, delim);
    if (str == NULL) {
        // This token is the last one
        *saveptr = token + strlen(token); // Point to the null terminator
    } else {
        // Terminate the token and save the pointer for the next call
        *str = '\0';
        *saveptr = str + 1;
    }

    return token;
}

// Implementation of strtok (non-reentrant, uses static variable)
// Note: This is generally unsafe in multi-threaded or reentrant scenarios.
// Prefer strtok_r if possible.
char* strtok(char *str, const char *delim) {
    static char *saveptr; // Static variable for state
    return strtok_r(str, delim, &saveptr);
}

// Helper function: strspn - Returns the length of the initial segment of s which consists entirely of bytes in accept
size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    const char *p;
    bool found;

    while (*s != '\0') {
        found = false;
        for (p = accept; *p != '\0'; p++) {
            if (*s == *p) {
                found = true;
                break;
            }
        }
        if (!found) {
            break;
        }
        count++;
        s++;
    }
    return count;
}

// Helper function: strpbrk - Returns a pointer to the first occurrence in s of any of the bytes in accept
char *strpbrk(const char *s, const char *accept) {
    const char *p;

    while (*s != '\0') {
        for (p = accept; *p != '\0'; p++) {
            if (*s == *p) {
                return (char *)s;
            }
        }
        s++;
    }
    return NULL;
}

