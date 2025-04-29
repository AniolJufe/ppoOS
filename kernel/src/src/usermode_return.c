#include "usermode_return.h"
#include "flanterm.h"
#include "serial.h"
#include "shell.h"

extern struct flanterm_context *ft_ctx;

void usermode_return_handler(int code) {
    // Print debug message
    serial_write("[USERMODE_RETURN] Handler called with code ", 43);
    serial_print_hex(code);
    serial_write("\n", 1);

    // Print message to terminal
    const char *msg = "\n[Process returned to kernel]\n";
    flanterm_write(ft_ctx, msg, 29);
    flanterm_flush(ft_ctx);
    serial_write(msg, 29);

    // Resume the shell without halting
    serial_write("[USERMODE_RETURN] Returning to shell\n", 37);
    shell_run();

    // This should never be reached, but just in case
    serial_write("[USERMODE_RETURN] Shell returned, halting\n", 41);
    for (;;) asm volatile("hlt");
}
