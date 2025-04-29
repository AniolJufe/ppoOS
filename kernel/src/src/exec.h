#pragma once

#include <stdbool.h>
#include <stddef.h>

// Execute an ELF file
// Returns true if execution was successful, false otherwise
void exec_elf(const char *filename);
