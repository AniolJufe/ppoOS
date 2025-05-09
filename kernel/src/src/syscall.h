#pragma once

#include <stdint.h>
#include <stddef.h>

// Syscall numbers
#define SYS_EXIT       0
#define SYS_WRITE      1
#define SYS_READ       2
#define SYS_OPEN       3
#define SYS_CLOSE      4
#define SYS_READDIR    5 // New syscall for reading directory entries
#define SYS_FORK       6 // Fork syscall

// File descriptor constants
#define STDIN_FD  0
#define STDOUT_FD 1
#define STDERR_FD 2

// Maximum file descriptors
#define MAX_FDS 16

// Structure for directory entry (used by SYS_READDIR)
// Matches simplified fs_file structure for now
struct dirent {
    char name[256]; // Max filename length
    uint64_t size;
    // Add other fields like type if needed later
};

// Standard C function signature for syscalls
typedef int64_t (*syscall_fn_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

// Function to handle syscalls - simple function call interface
// No assembly required, just a regular function call
int64_t syscall(int64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

// Initialize syscall infrastructure
void syscall_init(void);

