#include "limine_libc.h"

int main() {
    printf("Starting fork test program\n");
    
    int pid = fork();
    
    if (pid < 0) {
        printf("Fork failed!\n");
        return 1;
    }
    
    if (pid == 0) {
        // Child process
        printf("Child process: Hello from the child! My PID is 0\n");
    } else {
        // Parent process
        printf("Parent process: Hello from the parent! Child PID is %d\n", pid);
    }
    
    printf("Process %d exiting\n", pid == 0 ? 0 : pid);
    return 0;
}