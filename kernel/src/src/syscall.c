#include "syscall.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h> // Added for memcpy, strncpy
#include "kernel.h"
#include "fs.h"
#include "keyboard.h"
#include "serial.h" // Needed for serial_write in fork syscall
#include "flanterm.h" // Include the full definition of flanterm_context
#include "vmm.h"     // For vmm_get_current_address_space and vmm_switch_address_space
#include "shell.h"   // For shell_run
#include "exec.h"    // For exec_elf

// Define user memory layout constants (copied from exec.c)
#define USER_STACK_PAGES 8 // Number of pages for the stack (8 * 4KiB = 32KiB)
#define USER_STACK_TOP_VADDR  (0x80000000 - PAGE_SIZE) // Top page starts here (e.g., 0x7FFFF000)
#define USER_STACK_BOTTOM_VADDR (USER_STACK_TOP_VADDR - (USER_STACK_PAGES * PAGE_SIZE))

// External functions we'll need
extern struct flanterm_context *ft_ctx;
// extern void serial_write(const char *buf, size_t length); // Removed
extern void flanterm_write(struct flanterm_context *ctx, const char *buf, size_t count);
extern void flanterm_flush(struct flanterm_context *ctx);

// --- MSR Definitions ---
#define MSR_EFER        0xC0000080 // Extended Feature Enable Register
#define MSR_STAR        0xC0000081 // Legacy mode SYSCALL Target CS/SS and kernel CS/SS
#define MSR_LSTAR       0xC0000082 // Long mode SYSCALL Target RIP
#define MSR_FMASK       0xC0000084 // Long mode SYSCALL RFLAGS Mask

// --- GDT Selectors (Ensure these match your GDT!) ---
#define KERNEL_CODE_SELECTOR 0x08
#define KERNEL_DATA_SELECTOR 0x10
#define USER_CODE_SELECTOR   0x1B // From usermode_entry.asm (0x18 | 3)
#define USER_DATA_SELECTOR   0x23 // From usermode_entry.asm (0x20 | 3)

// External declaration for the assembly syscall entry point
extern void syscall_asm_entry(void);

// Helper function to write MSR
static inline void wrmsr(uint32_t msr_id, uint64_t value) {
    asm volatile (
        "wrmsr"
        : // no outputs
        : "c"(msr_id), "a"((uint32_t)value), "d"((uint32_t)(value >> 32))
        : "memory"
    );
}

// Helper function to read MSR
static inline uint64_t rdmsr(uint32_t msr_id) {
    uint32_t low, high;
    asm volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr_id)
        : "memory"
    );
    return ((uint64_t)high << 32) | low;
}

// Simple file descriptor table
struct file_descriptor {
    struct fs_file *file;
    size_t position;
    bool used;
} fd_table[MAX_FDS];

// Basic check: ensure address is below kernel space
// TODO: Implement robust user memory validation using page tables.
// This basic check is NOT sufficient for security or stability.
#define KERNEL_VMA_BASE 0xffff800000000000

static bool validate_user_memory(uint64_t vaddr, size_t size, bool write_access) {
    // Check for null pointer or zero size
    if (vaddr == 0 || size == 0) {
        return false;
    }

    // Check for overflow
    uint64_t end_addr;
    if (__builtin_add_overflow(vaddr, size - 1, &end_addr)) {
        return false; // Overflow occurred
    }

    // Basic check: ensure the entire range is below kernel base
    if (vaddr >= KERNEL_VMA_BASE || end_addr >= KERNEL_VMA_BASE) {
        return false;
    }

    // TODO: Add page table checks here!
    // Iterate through pages covered by [vaddr, end_addr]
    // For each page:
    // 1. Translate virtual address to physical address using current process's page tables.
    // 2. Check if the page table entry exists (present bit).
    // 3. Check if the user access bit is set.
    // 4. If write_access is true, check if the writeable bit is set.
    // If any check fails, return false.

    (void)write_access; // Mark as unused until page table checks are implemented

    return true; // Placeholder: Assume valid if basic checks pass
}

// Optimized copy function using memcpy, with basic validation.
// Returns number of bytes copied, or -1 on error (invalid address range).
static int64_t copy_from_user(void *kdest, const void *user_src, size_t size) {
    if (!validate_user_memory((uint64_t)user_src, size, false)) {
        // Consider logging this error via a kernel mechanism if available,
        // but avoid direct serial writes here.
        return -1; // Indicate error (EFAULT)
    }

    // TODO: Implement proper fault handling around this memcpy.
    // If a page fault occurs during the copy, it should be handled gracefully.
    memcpy(kdest, user_src, size);

    return (int64_t)size;
}

// Optimized copy function using memcpy, with basic validation.
// Returns number of bytes copied, or -1 on error (invalid address range).
static int64_t copy_to_user(void *user_dest, const void *ksrc, size_t size) {
    if (!validate_user_memory((uint64_t)user_dest, size, true)) {
        // Consider logging this error via a kernel mechanism if available,
        // but avoid direct serial writes here.
        return -1; // Indicate error (EFAULT)
    }

    // TODO: Implement proper fault handling around this memcpy.
    // If a page fault occurs during the copy, it should be handled gracefully.
    memcpy(user_dest, ksrc, size);

    return (int64_t)size;
}


// Syscall implementations
static int64_t sys_exit(uint64_t code, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; // Mark unused

    // Print exit message to terminal (Manual construction)
    // This is kept as it's user-visible output, not debug logging.
    char buf[64];
    const char *exit_msg = "\nProgram exited with code ";
    size_t i = 0;
    while (exit_msg[i]) {
        buf[i] = exit_msg[i];
        i++;
    }
    // Convert code to string (simple unsigned version)
    if (code == 0) {
        buf[i++] = '0';
    } else {
        uint64_t temp = code;
        size_t digits = 0;
        // Count digits
        while (temp > 0) {
            temp /= 10;
            digits++;
        }
        // Convert to string
        temp = code;
        size_t pos = i + digits - 1;
        size_t end_pos = pos + 1;
        while (temp > 0) {
            buf[pos--] = '0' + (temp % 10);
            temp /= 10;
        }
        i = end_pos;
    }
    buf[i++] = '\n';
    buf[i] = '\0';
    if (ft_ctx) { // Check if terminal context is available
        flanterm_write(ft_ctx, buf, i);
        flanterm_flush(ft_ctx);
    }

    // Return a special value to indicate process exit
    // This will be caught in the syscall handler
    return 0xDEAD; // Special marker for process exit
}

static int64_t sys_write(uint64_t fd, uint64_t buf_ptr, uint64_t count, uint64_t arg4, uint64_t arg5) {
    (void)arg4; (void)arg5; // Mark unused

    // Validate parameters
    if (count == 0) {
        return 0; // Writing 0 bytes is valid and does nothing
    }
    // Basic validation of buf_ptr is done in copy_from_user

    // Limit write size to prevent excessive kernel buffer usage for now.
    // A better approach would handle large writes in chunks.
    if (count > 4096) {
        count = 4096;
    }

    // Handle standard output / standard error
    if (fd == STDOUT_FD || fd == STDERR_FD) {
        // Allocate a temporary kernel buffer
        // NOTE: Using a fixed-size stack buffer is simple but risky for large counts.
        // Consider dynamic allocation or a dedicated buffer pool for robustness.
        char kbuf[4096]; // Use a kernel buffer matching the max count
        size_t bytes_to_copy = count;

        // Safely copy data from user space to kernel buffer
        int64_t copied_bytes = copy_from_user(kbuf, (const void *)buf_ptr, bytes_to_copy);

        if (copied_bytes < 0) {
            return -1; // Return error (EFAULT)
        }

        // Write the data from the kernel buffer to the terminal
        if (ft_ctx) { // Check if terminal context is available
            flanterm_write(ft_ctx, kbuf, (size_t)copied_bytes);
            flanterm_flush(ft_ctx);
        }

        return copied_bytes; // Return the number of bytes written
    }
    // Handle file output
    else if (fd >= 3 && fd < MAX_FDS && fd_table[fd].used) {
        struct file_descriptor *desc = &fd_table[fd];
        struct fs_file *file = desc->file;

        // TODO: Implement file writing. Requires fs_write or similar.
        // Need to copy data from user space first.
        // char kbuf[4096]; // Or use dynamic allocation
        // size_t bytes_to_write = count;
        // if (bytes_to_write > sizeof(kbuf)) bytes_to_write = sizeof(kbuf);
        // int64_t copied = copy_from_user(kbuf, (const void *)buf_ptr, bytes_to_write);
        // if (copied < 0) return -1;
        // int64_t written = fs_write(file, desc->position, kbuf, copied);
        // if (written > 0) desc->position += written;
        // return written;

        (void)desc; // Mark unused until implemented
        (void)file; // Mark unused until implemented
        return -1; // File writing not implemented yet
    }

    return -1; // Invalid fd or other error (EBADF)
}

static int64_t sys_read(uint64_t fd, uint64_t buf_ptr, uint64_t count, uint64_t arg4, uint64_t arg5) {
    (void)arg4; (void)arg5; // Mark unused

    if (count == 0) {
        return 0;
    }

    // ----------------------
    // Read from standard input
    // ----------------------
    if (fd == STDIN_FD) {
        size_t read_bytes = 0;
        char kbuf[4096];
        size_t to_read = count > sizeof(kbuf) ? sizeof(kbuf) : count;

        while (read_bytes < to_read) {
            char c = keyboard_read_char();
            if (!c)
                continue;
            // Translate carriage return to newline for convenience
            if (c == '\r')
                c = '\n';
            kbuf[read_bytes++] = c;
            if (c == '\n')
                break;
        }

        int64_t copied = copy_to_user((void *)buf_ptr, kbuf, read_bytes);
        if (copied < 0)
            return -1;
        return copied;
    }
    // ----------------------
    // Read from an opened file
    // ----------------------
    else if (fd >= 3 && fd < MAX_FDS && fd_table[fd].used) {
        struct file_descriptor *desc = &fd_table[fd];
        struct fs_file *file = desc->file;

        // Calculate bytes to read using filesystem helper
        size_t to_read = count;
        if (to_read > 4096)
            to_read = 4096;

        char kbuf[4096];
        size_t bytes_read = fs_read(file, desc->position, kbuf, to_read);

        if (bytes_read == 0)
            return 0; // EOF

        int64_t copied = copy_to_user((void *)buf_ptr, kbuf, bytes_read);
        if (copied < 0)
            return -1; // EFAULT

        desc->position += bytes_read;
        return copied;
    }

    return -1; // Invalid fd (EBADF)
}

static int64_t sys_open(uint64_t path_ptr, uint64_t flags, uint64_t mode, uint64_t arg4, uint64_t arg5) {
    (void)flags; (void)mode; (void)arg4; (void)arg5; // Mark unused (flags/mode ignored for now)

    // Find a free file descriptor (starting from 3)
    int fd = -1;
    for (int i = 3; i < MAX_FDS; i++) {
        if (!fd_table[i].used) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        return -1; // No free file descriptors (EMFILE)
    }

    // Copy path from user space
    // Assume max path length for simplicity. A better way involves dynamic allocation or checking size.
    char kpath[256];
    // Need to determine path length first, or copy byte-by-byte until null terminator
    // Simple approach: Assume null-terminated string within a reasonable limit
    // This is UNSAFE if the user provides a non-null-terminated string or invalid pointer!
    // TODO: Implement safe string copy from user space (e.g., strncpy_from_user)
    int64_t copied_len = copy_from_user(kpath, (const void *)path_ptr, sizeof(kpath) - 1);
    if (copied_len < 0) {
        return -1; // EFAULT
    }
    kpath[copied_len] = '\0'; // Ensure null termination

    // Find the null terminator within the copied buffer to get actual length
    size_t path_len = strnlen(kpath, sizeof(kpath));
    if (path_len >= sizeof(kpath) -1) { // Check if null terminator was actually found within bounds
        // Path might be too long or not null-terminated properly
        return -1; // ENAMETOOLONG or EFAULT
    }

    // Open the file using the kernel path
    struct fs_file *file = fs_open(kpath);

    if (file == NULL) {
        // TODO: Handle file creation based on flags (O_CREAT)
        return -1; // File not found (ENOENT)
    }

    // Initialize the file descriptor
    fd_table[fd].file = file;
    fd_table[fd].position = 0;
    fd_table[fd].used = true;

    return fd;
}

static int64_t sys_close(uint64_t fd, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5; // Mark unused

    // Check if fd is valid and in use (excluding stdin, stdout, stderr)
    if (fd >= 3 && fd < MAX_FDS && fd_table[fd].used) {
        // Mark as unused
        fd_table[fd].used = false;
        fd_table[fd].file = NULL; // Clear file pointer
        fd_table[fd].position = 0;
        // We don't actually 'close' the underlying fs_file here, assuming
        // the filesystem manages its lifetime. If needed, call fs_close(file).
        return 0; // Success
    }

    return -1; // Invalid fd (EBADF)
}

// New syscall: sys_readdir
// Reads the next directory entry from the filesystem.
// arg1 (index): The index of the directory entry to read.
// arg2 (buf_ptr): User space pointer to a struct dirent buffer.
// arg3 (buf_size): Size of the user space buffer.
// Returns: 1 on success, 0 if no more entries, -1 on error.
static int64_t sys_readdir(uint64_t index, uint64_t buf_ptr, uint64_t buf_size, uint64_t arg4, uint64_t arg5) {
    (void)arg4; (void)arg5; // Mark unused

    // Validate user buffer pointer and size
    if (buf_size < sizeof(struct dirent)) {
        return -1; // Buffer too small (EINVAL)
    }
    if (!validate_user_memory(buf_ptr, sizeof(struct dirent), true)) {
        return -1; // Invalid buffer pointer (EFAULT)
    }

    // Get the list of files from the filesystem
    size_t nfiles = 0;
    const struct fs_file *files = fs_list(&nfiles);

    // Check if the requested index is valid
    if (index >= nfiles) {
        return 0; // No more entries
    }

    // Get the file info for the requested index
    const struct fs_file *file_info = &files[index];

    // Prepare the dirent structure in kernel space
    struct dirent kdirent;
    memset(&kdirent, 0, sizeof(struct dirent));
    strncpy(kdirent.name, file_info->name, sizeof(kdirent.name) - 1);
    kdirent.name[sizeof(kdirent.name) - 1] = '\0'; // Ensure null termination
    kdirent.size = file_info->size;

    // Copy the dirent structure to user space
    int64_t copied_bytes = copy_to_user((void *)buf_ptr, &kdirent, sizeof(struct dirent));

    if (copied_bytes < 0) {
        return -1; // Error copying to user space (EFAULT)
    }

    // Return 1 to indicate success (one entry read)
    return 1;
}


// Structure to store process context for fork
struct fork_context {
    // Registers saved by syscall_asm_entry
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    // Return address (RIP) saved by syscall instruction
    uint64_t rip;
    // User stack pointer
    uint64_t rsp;
    // RFLAGS register
    uint64_t rflags;
};

// Simple PID counter for fork
static uint64_t next_pid = 1;

// External declarations needed for fork
extern void* syscall_stack_top; // From syscall_entry.asm
extern uint64_t user_rsp_storage; // From syscall_entry.asm
extern uint64_t user_rip_storage; // From syscall_entry.asm
extern uint64_t user_rflags_storage; // From syscall_entry.asm

// Implementation of the fork syscall
static int64_t sys_fork(uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5; // Mark unused

    serial_write("[FORK] Starting fork syscall\n", 29);

    // 1. Get the current process's address space
    pml4_t* parent_pml4 = vmm_get_current_address_space();
    if (!parent_pml4) {
        serial_write("[FORK] Error: Failed to get parent address space\n", 48);
        return -1;
    }

    // 2. Create a new address space for the child process
    pml4_t* child_pml4 = vmm_create_address_space();
    if (!child_pml4) {
        serial_write("[FORK] Error: Failed to create child address space\n", 50);
        return -1;
    }

    // 3. Save the current process context
    struct fork_context context;
    // Get user stack pointer and instruction pointer from syscall_entry.asm
    context.rsp = user_rsp_storage;
    context.rip = user_rip_storage;
    context.rflags = user_rflags_storage;
    
    // Get callee-saved registers from the stack
    // These were pushed in syscall_asm_entry in the order: rbp, rbx, r12, r13, r14, r15
    // The stack layout at this point is:
    // [syscall_stack_top - 8*1] -> r15
    // [syscall_stack_top - 8*2] -> r14
    // [syscall_stack_top - 8*3] -> r13
    // [syscall_stack_top - 8*4] -> r12
    // [syscall_stack_top - 8*5] -> rbx
    // [syscall_stack_top - 8*6] -> rbp
    uint64_t* stack_ptr = (uint64_t*)syscall_stack_top;
    context.r15 = *(stack_ptr - 1);
    context.r14 = *(stack_ptr - 2);
    context.r13 = *(stack_ptr - 3);
    context.r12 = *(stack_ptr - 4);
    context.rbx = *(stack_ptr - 5);
    context.rbp = *(stack_ptr - 6);

    // 4. Copy the parent's memory to the child
    // We need to iterate through the parent's address space and copy all user pages
    // For simplicity, we'll copy all pages below KERNEL_VMA_BASE
    // This is a simplified approach and would need to be improved for a real OS
    
    // Define the user address space range
    uint64_t user_start = 0;
    /*
     * The original implementation attempted to iterate all pages up to
     * KERNEL_VMA_BASE (0xffff800000000000). This results in an absurdly large
     * loop (trillions of iterations) and effectively locks the kernel during
     * fork.  User space in this project only occupies the lower 2GiB so limit
     * the copy range accordingly.  This keeps the loop bounded to roughly half
     * a million iterations while still covering all user pages.
     */
    uint64_t user_end = USER_STACK_TOP_VADDR + PAGE_SIZE;
    uint64_t page_size = PAGE_SIZE;
    
    // Iterate through the user address space
    for (uint64_t vaddr = user_start; vaddr < user_end; vaddr += page_size) {
        // Get the physical address for this virtual address in the parent
        uint64_t parent_phys = vmm_get_physical_address(parent_pml4, vaddr);
        
        // Skip if not mapped
        if (parent_phys == 0) {
            continue;
        }
        
        // Allocate a new physical frame for the child
        void* child_phys = pmm_alloc_frame();
        if (!child_phys) {
            serial_write("[FORK] Error: Out of memory during page copy\n", 45);
            // TODO: Clean up already allocated pages
            return -1;
        }
        
        // Copy the page content
        // Convert physical addresses to kernel virtual addresses for access
        void* parent_virt = phys_to_virt(parent_phys);
        void* child_virt = phys_to_virt((uint64_t)child_phys);
        memcpy(child_virt, parent_virt, page_size);
        
        // Get the page flags from the parent
        // For simplicity, we'll use the same flags
        // In a real OS, you might want to handle copy-on-write here
        uint64_t flags = PTE_PRESENT | PTE_USER;
        
        // Check if the page is writable in the parent
        // This is a simplified approach - in a real OS you would get the actual flags
        if (vaddr >= USER_STACK_BOTTOM_VADDR && vaddr <= USER_STACK_TOP_VADDR) {
            // Stack pages are writable
            flags |= PTE_WRITABLE;
        }
        
        // Map the page in the child's address space
        if (!vmm_map_page(child_pml4, vaddr, (uint64_t)child_phys, flags)) {
            serial_write("[FORK] Error: Failed to map page in child\n", 42);
            pmm_free_frame(child_phys);
            // TODO: Clean up already allocated pages
            return -1;
        }
    }
    
    // 5. Assign a PID to the child
    uint64_t child_pid = next_pid++;
    
    // 6. Set up the child to return 0 from fork
    // This is done by modifying the saved context that will be restored when the child runs
    
    // 7. Return the child's PID to the parent
    serial_write("[FORK] Fork successful, child PID: ", 34);
    // Convert PID to string and print it
    char pid_str[20];
    int i = 0;
    uint64_t temp = child_pid;
    if (temp == 0) {
        pid_str[i++] = '0';
    } else {
        // Count digits
        int digits = 0;
        uint64_t temp_copy = temp;
        while (temp_copy > 0) {
            temp_copy /= 10;
            digits++;
        }
        
        // Convert to string (backwards)
        i = digits - 1;
        while (temp > 0) {
            pid_str[i--] = '0' + (temp % 10);
            temp /= 10;
        }
        i = digits;
    }
    pid_str[i] = '\0';
    serial_write(pid_str, i);
    serial_write("\n", 1);
    
    // Store the child's context and address space for later use
    // In a real OS, you would have a process table to store this information
    // For simplicity, we'll just return the PID and handle the child execution separately
    
    // TODO: Implement a proper process table and scheduler
    // For now, we'll use a simple approach where the child executes after the parent returns
    
    return child_pid;
}

// Syscall function pointers
// Ensure the order matches the SYS_ constants in syscall.h
static syscall_fn_t syscall_table[] = {
    [SYS_EXIT]    = sys_exit,
    [SYS_WRITE]   = sys_write,
    [SYS_READ]    = sys_read,
    [SYS_OPEN]    = sys_open,
    [SYS_CLOSE]   = sys_close,
    [SYS_READDIR] = sys_readdir,
    [SYS_FORK]    = sys_fork, // Add the fork syscall handler
    // Add other syscalls here as they are implemented
};

// Calculate table size dynamically, but ensure it's large enough for highest syscall number
#define MAX_SYSCALL_NUM SYS_FORK
#define SYSCALL_TABLE_SIZE (MAX_SYSCALL_NUM + 1)

// Main syscall handler - called from assembly
__attribute__((visibility("default"))) int64_t syscall(int64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5) {

    // Validate syscall number
    if (num < 0 || (uint64_t)num >= SYSCALL_TABLE_SIZE || syscall_table[num] == NULL) {
        // Invalid syscall number or unimplemented syscall
        return -1; // Or a specific error code like ENOSYS
    }

    // Dispatch to the appropriate syscall handler
    syscall_fn_t handler = syscall_table[num];
    int64_t result = handler(arg1, arg2, arg3, arg4, arg5);

    // Check for special exit marker from sys_exit
    if (result == 0xDEAD && num == SYS_EXIT) {
        // This indicates the process should terminate and return to the shell.
        // The actual context switch back to the shell needs to be handled
        // after the syscall returns to the assembly stub, or via a scheduler.
        // For now, we rely on the shell loop re-invoking shell_run.
        // We still need to return something to the iretq instruction.
        // Returning 0 is conventional for exit, though it won't be used by the exiting process.
        return 0;
    }

    return result;
}

// Initialize SYSCALL/SYSRET MSRs
void syscall_init(void) {
    // Enable SCE (SysCall Enable) bit in EFER MSR
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= 1; // Set SCE bit (bit 0)
    wrmsr(MSR_EFER, efer);

    // Set SYSCALL target RIP (LSTAR MSR)
    wrmsr(MSR_LSTAR, (uint64_t)syscall_asm_entry);

    // Set SYSCALL target CS/SS (STAR MSR)
    // According to AMD manual Vol 2, section 4.6:
    // STAR[47:32] = SYSCALL target CS selector (Kernel CS)
    // STAR[63:48] = SYSCALL target SS selector - 8 (Kernel SS - 8)
    // Kernel CS = 0x08, Kernel SS = 0x10
    // STAR = ( (KERNEL_DATA_SELECTOR) << 48 ) | ( KERNEL_CODE_SELECTOR << 32 )
    // This seems wrong based on manual. Let's use the derived values:
    // Target CS = KERNEL_CODE_SELECTOR (0x08)
    // Target SS = KERNEL_DATA_SELECTOR (0x10)
    // STAR[63:48] should be SS selector, STAR[47:32] should be CS selector.
    // The SYSRET instruction uses CS = STAR[63:48]+16 and SS = STAR[63:48]+8.
    // For SYSRET to return to user mode (CS=0x1B, SS=0x23), STAR[63:48] must be 0x13?
    // Let's stick to the common setup: Kernel CS in 47:32, Kernel SS in 63:48
    // STAR = (KERNEL_SS << 48) | (KERNEL_CS << 32)
    uint64_t star = ((uint64_t)KERNEL_DATA_SELECTOR << 48) | ((uint64_t)KERNEL_CODE_SELECTOR << 32);
    wrmsr(MSR_STAR, star);

    // Set RFLAGS mask (FMASK MSR)
    // Mask flags that should be cleared on syscall entry (e.g., IF, TF, DF)
    // Common mask: 0x3F7FD5 (clears TF, IF, DF, NT, RF, VM, AC, VIF, VIP, ID)
    // Clear IF (Interrupt Flag) to disable interrupts during syscall
    // Clear DF (Direction Flag) for string operations
    // Clear TF (Trap Flag) to disable single-stepping
    wrmsr(MSR_FMASK, 0x700); // Clear IF (bit 9), DF (bit 10), TF (bit 8)

    // Initialize file descriptor table (stdin, stdout, stderr are implicitly handled)
    for (int i = 0; i < MAX_FDS; i++) {
        fd_table[i].used = false;
        fd_table[i].file = NULL;
        fd_table[i].position = 0;
    }

    // Initialize PID counter
    next_pid = 1;

    // Note: No serial prints here
}

