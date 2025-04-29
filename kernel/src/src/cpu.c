#include "cpu.h"
#include "syscall.h"
#include "serial.h"

// External function from syscall_entry.asm
extern void syscall_asm_entry(void);

// Setup CPU for syscalls
void cpu_init(void) {
    // Enable syscall/sysret in EFER MSR
    uint64_t efer = read_msr(MSR_EFER);
    efer |= EFER_SCE;
    write_msr(MSR_EFER, efer);
    
    // Setup STAR MSR (segments for syscall/sysret)
    // Format: [63:48] = user code selector (0x18), [47:32] = kernel code selector (0x08)
    // Use RPL=0 selectors: kernel=0x08, user=0x18
    uint64_t star = (0x18ULL << 48) | (0x08ULL << 32);
    write_msr(MSR_STAR, star);
    
    // Setup LSTAR MSR (syscall entry point)
    write_msr(MSR_LSTAR, (uint64_t)syscall_asm_entry);
    
    // Setup FMASK MSR (flags mask for syscall)
    // Disable interrupts during syscall by masking IF flag (bit 9)
    write_msr(MSR_FMASK, (1 << 9));
    
    // Initialize syscall handler
    syscall_init();
}

// Debug function callable from assembly to print iretq stack contents
void debug_print_iretq_frame(uint64_t rip, uint64_t cs, uint64_t rflags, uint64_t rsp, uint64_t ss) {
    serial_write("iretq frame (before jump):\n", 29);
    serial_write("  RIP:    ", 10);
    serial_print_hex(rip);
    serial_write("\n", 1);
    serial_write("  CS:     ", 10);
    serial_print_hex(cs);
    serial_write("\n", 1);
    serial_write("  RFLAGS: ", 10);
    serial_print_hex(rflags);
    serial_write("\n", 1);
    serial_write("  RSP:    ", 10);
    serial_print_hex(rsp);
    serial_write("\n", 1);
    serial_write("  SS:     ", 10);
    serial_print_hex(ss);
    serial_write("\n", 1);
}
