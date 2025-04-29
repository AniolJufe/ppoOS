#include "limine_libc/stdio.h"
#include "limine_libc/syscall.h"
#include "limine_libc/string.h"

int main(int argc, char *argv[]) {
    (void)argc; // Mark unused for now
    (void)argv; // Mark unused for now

    struct dirent entry;
    int result;
    unsigned int index = 0;

    // Loop through directory entries using the readdir syscall
    while ((result = readdir(index, &entry)) == 1) {
        // Print the entry name
        printf("%s  ", entry.name);
        index++;
    }

    // Print a newline after listing all entries
    printf("\n");

    if (result < 0) {
        // Handle potential errors from readdir if needed
        printf("ls: Error reading directory\n");
        return 1;
    }

    return 0; // Success
}

