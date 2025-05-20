#include <syscall.h>

int main() {
    // Use a normal string buffer for writing
    const char message[] = "Normal write test message";
    
    // Write the message to stdout
    int result = write(1, message, sizeof(message) - 1);
    
    // Return the number of bytes written
    return result;
}
