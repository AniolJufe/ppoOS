#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h> // Include for size_t

// Syscall numbers (must match kernel)
#define SYS_EXIT       0
#define SYS_WRITE      1
#define SYS_READ       2
#define SYS_OPEN       3
#define SYS_CLOSE      4
#define SYS_READDIR    5 // New syscall for reading directory entries
#define SYS_FORK       6 // Fork syscall

#define STDIN   0
#define STDOUT  1
#define STDERR  2

// Structure for directory entry (must match kernel)
struct dirent {
    char name[256]; // Max filename length
    uint64_t size;
    // Add other fields like type if needed later
};

// Syscall wrapper function prototypes
int write(int fd, const void *buf, size_t count);
void exit(int status);
int read(int fd, void *buf, size_t count);
int open(const char *pathname, int flags);
int close(int fd);
int readdir(unsigned int index, struct dirent *dirp); // Wrapper for SYS_READDIR
int fork(void); // Wrapper for SYS_FORK

#endif // SYSCALL_H

