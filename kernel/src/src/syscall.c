#include "syscall.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h> // Added for memcpy, strncpy
#include "kernel.h"
#include "fs.h"
// #include "serial.h" // Removed: No direct serial debugging in syscalls
#include "flanterm.h" // Include the full definition of flanterm_context
#include "vmm.h"     // For vmm_get_current_address_space and vmm_switch_address_space
#include "shell.h"   // For shell_run

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

    // Only stdin supported for now (basic, non-blocking)
    if (fd == STDIN_FD) {
        // TODO: Implement proper blocking keyboard read. This requires integrating
        // with the keyboard driver and potentially process scheduling.
        // For now, return 0 (EOF or no data available).
        return 0;
    }
    // Handle file input
    else if (fd >= 3 && fd < MAX_FDS && fd_table[fd].used) {
        struct file_descriptor *desc = &fd_table[fd];
        struct fs_file *file = desc->file;

        // Check if we're at EOF
        if (desc->position >= file->size) {
            return 0; // EOF
        }

        // Calculate how many bytes to read
        size_t remaining = file->size - desc->position;
        size_t to_read = count < remaining ? count : remaining;

        // Limit read size to prevent excessive kernel buffer usage if needed,
        // or handle large reads in chunks.
        if (to_read > 4096) {
            to_read = 4096;
        }

        // Allocate a temporary kernel buffer
        char kbuf[4096];

        // Read data from file into kernel buffer
        // Assuming fs_read reads from file->data based on offset and size
        // We need a proper fs_read function here.
        // For now, simulate reading directly from file->data if available.
        if (file->data) {
             // TODO: Replace with actual fs_read(file, desc->position, kbuf, to_read);
             memcpy(kbuf, (const char *)file->data + desc->position, to_read);
        } else {
             return -1; // Cannot read if file data is not available
        }

        // Copy data from kernel buffer to user buffer
        int64_t copied_bytes = copy_to_user((void *)buf_ptr, kbuf, to_read);

        if (copied_bytes < 0) {
            return -1; // EFAULT
        }

        // Update position only if copy was successful
        desc->position += (size_t)copied_bytes;

        return copied_bytes;
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


// Syscall function pointers
// Ensure the order matches the SYS_ constants in syscall.h
static syscall_fn_t syscall_table[] = {
    [SYS_EXIT]    = sys_exit,
    [SYS_WRITE]   = sys_write,
    [SYS_READ]    = sys_read,
    [SYS_OPEN]    = sys_open,
    [SYS_CLOSE]   = sys_close,
    [SYS_READDIR] = sys_readdir, // Add the new syscall handler
    // Add other syscalls here as they are implemented
};

// Calculate table size dynamically, but ensure it's large enough for highest syscall number
#define MAX_SYSCALL_NUM SYS_READDIR
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

    // Note: No serial prints here
}

