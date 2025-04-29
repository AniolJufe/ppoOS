#pragma once

#include <stdint.h>

// Define the structure for an IDT entry (Gate Descriptor)
struct idt_entry {
    uint16_t offset_low;    // Lower 16 bits of handler function address
    uint16_t selector;      // Code segment selector (usually 0x08)
    uint8_t  ist;           // Interrupt Stack Table (0 for now)
    uint8_t  type_attr;     // Type and attributes (e.g., P, DPL, Type)
    uint16_t offset_mid;    // Middle 16 bits of handler function address
    uint32_t offset_high;   // Upper 32 bits of handler function address
    uint32_t zero;          // Reserved, must be zero
} __attribute__((packed));

// Define the structure for the IDT pointer (used with lidt)
struct idt_ptr {
    uint16_t limit;         // Size of the IDT in bytes - 1
    uint64_t base;          // Linear address of the IDT
} __attribute__((packed));

// Structure to hold register state pushed by the ISR stub
// Matches the order of pushes in the common stub
struct registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rsi, rdi, rbp, rdx, rcx, rbx, rax; // Pushed by common stub
    uint64_t int_no, err_code;                   // Pushed by specific stub or CPU
    uint64_t rip, cs, rflags, rsp, ss;           // Pushed by CPU on interrupt
};

// Constants for type_attr field
#define IDT_TA_InterruptGate 0x8E // P=1, DPL=0, Type=0xE (32-bit Interrupt Gate)
#define IDT_TA_TrapGate      0x8F // P=1, DPL=0, Type=0xF (32-bit Trap Gate)
// Note: We use 32-bit gate types even in 64-bit mode for interrupts/exceptions

// Function Declarations
extern void idt_init(void);
extern void isr_handler(struct registers *regs);

// Declare the external assembly ISR stubs (will be defined in isr_stubs.asm)
// We need stubs for the exceptions we want to handle.
extern void isr13(void); // General Protection Fault (#GP)
extern void isr14(void); // Page Fault (#PF)

// Add declarations for other ISRs if needed
