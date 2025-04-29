#include "serial.h"
#include <stdint.h>

// Basic x86 OUT instruction
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

// Basic x86 IN instruction
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

#define PORT 0x3f8   // COM1

void serial_init() {
   outb(PORT + 1, 0x00);    // Disable all interrupts
   outb(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
   outb(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
   outb(PORT + 1, 0x00);    //                  (hi byte)
   outb(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
   outb(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
   outb(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

int is_transmit_empty() {
   return inb(PORT + 5) & 0x20;
}

void serial_write_char(char a) {
   while (is_transmit_empty() == 0);
   outb(PORT, a);
}

void serial_write(const char *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        serial_write_char(data[i]);
    }
}

// Print a 64-bit number in hexadecimal format
void serial_print_hex(uint64_t n) {
    char buffer[17]; // 16 hex digits + null terminator
    const char hex_chars[] = "0123456789abcdef";
    buffer[16] = '\0';
    serial_write("0x", 2);
    if (n == 0) {
        serial_write_char('0');
        return;
    }

    int i = 15;
    while (n > 0 && i >= 0) {
        buffer[i--] = hex_chars[n % 16];
        n /= 16;
    }
    while (i >= 0) {
        buffer[i--] = '0'; // Pad with leading zeros if needed
    }

    serial_write(buffer, 16);
}
