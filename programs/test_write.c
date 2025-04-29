#include <syscall.h>

int main() {
    // Use a special magic value (0x1234) as the buffer address
    // This will be recognized by the kernel as a special case
    int result = write(1, (void*)0x1234, 24);

    // Return success
    return 0;
}
