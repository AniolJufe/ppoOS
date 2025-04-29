#include "idt.h"
#include "serial.h"
#include "lib/string.h"
#include <stdbool.h> // Include for bool type
#include "vmm.h"     // Include for pml4_t and vmm function prototypes

// Declare the IDT array (256 entries)
static struct idt_entry idt_entries[256];

// Declare the IDT pointer structure
static struct idt_ptr idt_pointer;

// External assembly function to load the IDT
extern void idt_load(struct idt_ptr *idt_ptr_addr);

// Function to set an entry in the IDT
static void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt_entries[num].offset_low = (base & 0xFFFF);
    idt_entries[num].offset_mid = ((base >> 16) & 0xFFFF);
    idt_entries[num].offset_high = ((base >> 32) & 0xFFFFFFFF);

    idt_entries[num].selector = sel;
    idt_entries[num].ist = 0; // Interrupt Stack Table (not used for now)
    idt_entries[num].type_attr = flags;
    idt_entries[num].zero = 0;
}

// Forward declarations
extern void shell_run(void);
extern struct flanterm_context *ft_ctx;
extern void flanterm_write(struct flanterm_context *ctx, const char *buf, size_t count);
extern void flanterm_flush(struct flanterm_context *ctx);

// C-level ISR handler called by assembly stubs
void isr_handler(struct registers *regs) {
    // If the fault is from user mode, try to recover by returning to the shell
    // Check the User/Supervisor bit in the error code for PF, or CS selector for others
    bool user_fault = false;
    if (regs->int_no == 14) { // Page Fault
        user_fault = (regs->err_code & 0x4);
    } else { // Other exceptions (like GP fault #13)
        user_fault = (regs->cs == 0x1b); // Check if CS is user code segment
    }

    if (user_fault) {
        serial_write("\n--- User Mode Fault ---\n", 26);
        serial_write(" INT: 0x", 8); serial_print_hex(regs->int_no);
        serial_write(", ERR: 0x", 8); serial_print_hex(regs->err_code);
        serial_write(" at RIP=0x", 10); serial_print_hex(regs->rip);
        serial_write("\n", 1);

        if (regs->int_no == 14) { // Page Fault specific info
            uint64_t faulting_address;
            asm volatile("mov %%cr2, %0" : "=r" (faulting_address));
            serial_write(" #PF accessing address 0x", 26); serial_print_hex(faulting_address);
            serial_write("\n", 1);
        }

        // Print message to console (if possible)
        const char* fault_msg = "\nUser process fault. Returning to shell.\n";
        //flanterm_write(ft_ctx, fault_msg, strlen(fault_msg));
        flanterm_flush(ft_ctx);
        serial_write(fault_msg, strlen(fault_msg)); // Also log to serial

        // Clean up? (e.g., free process memory - requires process management)
        // For now, we just abandon the process state.

        // Restore kernel address space
        if (!g_kernel_pml4) {
            serial_write("[ISR_HANDLER] FATAL: g_kernel_pml4 is NULL!\n", 46);
            goto halt_system; // Cannot recover without kernel PML4
        }
        serial_write("[ISR_HANDLER] Switching back to kernel PML4\n", 42);
        vmm_switch_address_space(g_kernel_pml4); 

        // Re-enable interrupts and jump to shell
        serial_write("[ISR_HANDLER] Enabling interrupts and running shell\n", 51);
        asm volatile ("sti");
        shell_run();

        // Should not be reached if shell_run loops indefinitely
        serial_write("[ISR_HANDLER] shell_run returned?! Halting.\n", 44);
        goto halt_system;
    }

    // --- Kernel Mode Fault or Unhandled Interrupt --- 
    serial_write("\n--- Kernel Fault or Unhandled Interrupt ---\n", 45);
    serial_write(" INT: 0x", 8); serial_print_hex(regs->int_no);
    serial_write(", ERR: 0x", 8); serial_print_hex(regs->err_code);
    serial_write("\n", 1);

    // Print specific messages for known faults
    if (regs->int_no == 13) { // General Protection Fault
        serial_write(" #GP: General Protection Fault\n", 30);
        serial_write("   Error Code: 0x", 16); serial_print_hex(regs->err_code);
        serial_write(" (usually segment selector index or 0)\n", 39);
    } else if (regs->int_no == 14) { // Page Fault
        uint64_t faulting_address;
        asm volatile("mov %%cr2, %0" : "=r" (faulting_address));
        serial_write(" #PF: Page Fault\n", 18);
        serial_write("   Faulting Address: 0x", 24); serial_print_hex(faulting_address);
        serial_write("\n   Error Code: 0x", 17); serial_print_hex(regs->err_code);
        serial_write("\n     -> ", 7);
        if (regs->err_code & 0x1) serial_write("P=1 (present) ", 15); else serial_write("P=0 (not present) ", 19);
        if (regs->err_code & 0x2) serial_write("W=1 (write) ", 13); else serial_write("R=1 (read) ", 12);
        if (regs->err_code & 0x4) serial_write("U=1 (user) ", 12); else serial_write("S=1 (supervisor) ", 18);
        if (regs->err_code & 0x8) serial_write("RSVD=1 (reserved bit) ", 24);
        if (regs->err_code & 0x10) serial_write("I/D=1 (instruction fetch)", 26);
        serial_write("\n", 1);
    }

    // Print common registers (for kernel faults)
    serial_write(" Kernel Registers:\n", 19); // Fixed string literal
    serial_write("   RIP: 0x", 10); serial_print_hex(regs->rip); serial_write(" CS: 0x", 7); serial_print_hex(regs->cs); serial_write(" RFLAGS: 0x", 11); serial_print_hex(regs->rflags); serial_write("\n", 1);
    serial_write("   RSP: 0x", 10); serial_print_hex(regs->rsp); serial_write(" SS: 0x", 7); serial_print_hex(regs->ss); serial_write("\n", 1);
    serial_write("   RAX: 0x", 10); serial_print_hex(regs->rax); serial_write(" RBX: 0x", 7); serial_print_hex(regs->rbx); serial_write(" RCX: 0x", 7); serial_print_hex(regs->rcx); serial_write(" RDX: 0x", 7); serial_print_hex(regs->rdx); serial_write("\n", 1);
    serial_write("   RSI: 0x", 10); serial_print_hex(regs->rsi); serial_write(" RDI: 0x", 7); serial_print_hex(regs->rdi); serial_write(" RBP: 0x", 7); serial_print_hex(regs->rbp); serial_write("\n", 1);
    serial_write("    R8: 0x", 10); serial_print_hex(regs->r8);  serial_write("  R9: 0x", 7); serial_print_hex(regs->r9);  serial_write(" R10: 0x", 7); serial_print_hex(regs->r10); serial_write(" R11: 0x", 7); serial_print_hex(regs->r11); serial_write("\n", 1);
    serial_write("   R12: 0x", 10); serial_print_hex(regs->r12); serial_write(" R13: 0x", 7); serial_print_hex(regs->r13); serial_write(" R14: 0x", 7); serial_print_hex(regs->r14); serial_write(" R15: 0x", 7); serial_print_hex(regs->r15); serial_write("\n", 1);

halt_system: // Moved label here so it's reachable from the user fault path too
    serial_write("System Halted.\n", 15);
    // Halt the system
    asm volatile ("cli; hlt");
}

// Initialize the IDT
void idt_init(void) {
    serial_write("IDT: Initializing...\n", 23);

    // Set up the IDT pointer
    idt_pointer.limit = sizeof(idt_entries) - 1;
    idt_pointer.base = (uint64_t)&idt_entries[0];

    // Zero out the IDT entries
    memset(&idt_entries[0], 0, sizeof(idt_entries));

    // Set gates for the exceptions we handle (using kernel code selector 0x08)
    // Use IDT_TA_InterruptGate for interrupts/exceptions
    idt_set_gate(13, (uint64_t)isr13, 0x08, IDT_TA_InterruptGate);
    idt_set_gate(14, (uint64_t)isr14, 0x08, IDT_TA_InterruptGate);

    // Add other ISRs here if needed

    // Load the IDT
    idt_load(&idt_pointer);

    serial_write("IDT: Loaded.\n", 14);
}
