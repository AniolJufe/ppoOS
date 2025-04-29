#pragma once

#include <stdint.h>

// MSR registers for syscall/sysret
#define MSR_EFER       0xC0000080
#define MSR_STAR       0xC0000081
#define MSR_LSTAR      0xC0000082
#define MSR_FMASK      0xC0000084

// EFER flags
#define EFER_SCE       (1 << 0)    // Syscall Enable

// Function to read MSR
static inline uint64_t read_msr(uint32_t msr) {
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

// Function to write MSR
static inline void write_msr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

// Setup CPU features
void cpu_init(void);
