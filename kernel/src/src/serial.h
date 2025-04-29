#ifndef SERIAL_H
#define SERIAL_H

#include <stddef.h>
#include <stdint.h>

void serial_init();
void serial_write_char(char a);
void serial_write(const char *data, size_t size);
void serial_print_hex(uint64_t n);

#endif // SERIAL_H
