#pragma once

#include <stdint.h>

// Defined in usermode_entry.asm
extern void jmp_usermode(uint64_t user_rip, uint64_t user_rsp);
