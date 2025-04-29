#include <stdio.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return 1;
    }
    
    // Open the file
    int fd = open(argv[1], 0); // 0 for read-only
    if (fd < 0) {
        printf("Error: Could not open file %s\n", argv[1]);
        return 1;
    }
    
    // Read and print the file contents
    char buffer[1024];
    int bytes_read;
    
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    
    // Close the file
    close(fd);
    
    return 0;
}
