#include "syscall.h"
#include <stdint.h>

// Syscall function (extern, implemented in assembly or elsewhere)
extern int64_t _syscall(int64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

void exit(int status) {
    _syscall(SYS_EXIT, status, 0, 0, 0, 0);
    // Loop indefinitely if exit fails for some reason
    for (;;) {
        _syscall(SYS_EXIT, 0xDEAD, 0, 0, 0, 0);
        __asm__ volatile("hlt");
    }
}

int write(int fd, const void *buf, size_t count) {
    return _syscall(SYS_WRITE, fd, (uint64_t)buf, count, 0, 0);
}

int read(int fd, void *buf, size_t count) {
    return _syscall(SYS_READ, fd, (uint64_t)buf, count, 0, 0);
}

int open(const char *pathname, int flags) {
    // Flags are currently ignored by the kernel syscall
    return _syscall(SYS_OPEN, (uint64_t)pathname, flags, 0, 0, 0);
}

int close(int fd) {
    return _syscall(SYS_CLOSE, fd, 0, 0, 0, 0);
}

// Wrapper for the new SYS_READDIR syscall
// Reads the directory entry at the given index.
// Returns 1 on success, 0 if no more entries, -1 on error.
int readdir(unsigned int index, struct dirent *dirp) {
    // Pass the index and the user buffer pointer to the kernel.
    // The kernel expects the size of the buffer implicitly via the struct definition.
    return _syscall(SYS_READDIR, index, (uint64_t)dirp, sizeof(struct dirent), 0, 0);
}


// These seem like remnants or incorrect implementations, removing them.
/*
int fopen(const char *pathname, const char *mode) {
    return _syscall(SYS_OPEN, (uint64_t)pathname, 0, 0, 0, 0);
}

int fclose(int fd) {
    return _syscall(SYS_CLOSE, fd, 0, 0, 0, 0);
}
*/

