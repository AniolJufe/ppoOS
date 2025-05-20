#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "kernel.h"
#include "keyboard.h"
#include <flanterm.h>
#include <string.h>
#include "fs.h"
#include "elf.h"
#include "exec.h"
#include "syscall.h"
#include "gui.h"

extern struct gui_context gui_ctx;

extern struct flanterm_context *ft_ctx;

#define SHELL_BUFSZ 256
#define SHELL_MAX_ARGS 8
#define SHELL_MAX_ARG_LEN 64

// ANSI Color Codes
#define ANSI_RESET     "\033[0m"
#define ANSI_BOLD      "\033[1m"
#define ANSI_ITALIC    "\033[3m"
#define ANSI_UNDERLINE "\033[4m"

#define ANSI_BLACK     "\033[30m"
#define ANSI_RED       "\033[31m"
#define ANSI_GREEN     "\033[32m"
#define ANSI_YELLOW    "\033[33m"
#define ANSI_BLUE      "\033[34m"
#define ANSI_MAGENTA   "\033[35m"
#define ANSI_CYAN      "\033[36m"
#define ANSI_WHITE     "\033[37m"

#define ANSI_BG_BLACK   "\033[40m"
#define ANSI_BG_RED     "\033[41m"
#define ANSI_BG_GREEN   "\033[42m"
#define ANSI_BG_YELLOW  "\033[43m"
#define ANSI_BG_BLUE    "\033[44m"
#define ANSI_BG_MAGENTA "\033[45m"
#define ANSI_BG_CYAN    "\033[46m"
#define ANSI_BG_WHITE   "\033[47m"

static void shell_print(const char *s) {
    if (!ft_ctx) return;
    size_t len = 0;
    while (s[len]) len++;
    flanterm_write(ft_ctx, s, len);
}

static void shell_print_colored(const char *s, const char *color) {
    shell_print(color);
    shell_print(s);
    shell_print(ANSI_RESET);
}

// Removed pim_editor function - `pim` will be an external command

// Check if a string ends with a suffix
static bool str_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    
    if (suffix_len > str_len) return false;
    
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

// Parse redirection operators in command line arguments
// NOTE: This is kept for potential future use, but redirection needs
// proper integration with process file descriptors, which is not fully
// implemented in the current syscalls/exec.
static bool parse_redirection(char *argv[], int *argc_ptr, char **outfile, bool *append_mode) {
    int argc = *argc_ptr;
    *outfile = NULL;
    *append_mode = false;
    
    // Look for redirection operators
    for (int i = 0; i < argc - 1; i++) {
        if (strcmp(argv[i], ">") == 0) {
            // Output redirection (overwrite)
            *outfile = argv[i + 1];
            *append_mode = false;
            
            // Remove redirection operator and filename from args
            for (int j = i; j < argc - 2; j++) {
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            *argc_ptr = argc;
            return true;
        } else if (strcmp(argv[i], ">>") == 0) {
            // Output redirection (append)
            *outfile = argv[i + 1];
            *append_mode = true;
            
            // Remove redirection operator and filename from args
            for (int j = i; j < argc - 2; j++) {
                argv[j] = argv[j + 2];
            }
            argc -= 2;
            *argc_ptr = argc;
            return true;
        }
    }
    
    return false;
}

// Tries to execute a command as an ELF executable.
// Searches in common paths like /bin/.
// Returns true if execution was attempted (even if it failed later),
// false if the file wasn't found for execution.
static bool try_exec_elf_command(const char *cmd, char *argv[], int argc) {
    // TODO: Implement proper PATH handling
    // For now, try prepending "/bin/"
    char path_buffer[256];
    const char *paths_to_try[] = {
        "/bin/",
        "/usr/bin/",
        "./", // Current directory
        ""
    };

    for (size_t i = 0; i < sizeof(paths_to_try)/sizeof(paths_to_try[0]); ++i) {
        // Construct full path
        strncpy(path_buffer, paths_to_try[i], sizeof(path_buffer) - 1);
        strncat(path_buffer, cmd, sizeof(path_buffer) - strlen(path_buffer) - 1);
        path_buffer[sizeof(path_buffer) - 1] = 0; // Ensure null termination

        // Check if the file exists in the filesystem
        struct fs_file *file = fs_open(path_buffer);
        if (file != NULL) {
            // File found, attempt execution
            // TODO: Pass argv/argc to the new process. This requires modifying
            // exec_elf and the process startup code to place argv/argc somewhere
            // accessible by the user program (e.g., on its stack).
            (void)argv; // Mark unused for now
            (void)argc; // Mark unused for now
            
            shell_print("Executing: ");
            shell_print(path_buffer);
            shell_print("\n");
            
            exec_elf(path_buffer);
            
            // If exec_elf returns, it means the process finished or failed to start.
            // The cleanup (switching back CR3, etc.) is handled within exec_elf.
            return true; // Indicate execution was attempted
        }
    }

    return false; // Command not found in any search path
}


static void shell_exec(const char *cmd, char *argv[], int argc) {
    // Removed: serial_write("IN SHELL MAIN\n", 15);
    if (argc == 0 || cmd[0] == 0) return;
    
    // Redirection parsing is kept but not fully functional without FD syscalls
    char *outfile = NULL;
    bool append_mode = false;
    bool has_redirection = parse_redirection(argv, &argc, &outfile, &append_mode);
    (void)outfile; // Mark unused for now
    (void)append_mode; // Mark unused for now
    (void)has_redirection; // Mark unused for now

    // --- Built-in Commands --- 
    if (!strcmp(cmd, "help")) {
        // Keep help concise, list available builtins and mention external commands
        shell_print_colored("╔═════════════════════════════════════╗\n", ANSI_CYAN);
        shell_print_colored("║ ", ANSI_CYAN);
        shell_print_colored("Limine Shell", ANSI_BOLD);
        shell_print_colored("                       ║\n", ANSI_CYAN);
        shell_print_colored("╠═════════════════════════════════════╣\n", ANSI_CYAN);
        shell_print_colored("║ ", ANSI_CYAN);
        shell_print_colored("Built-in Commands:", ANSI_YELLOW);
        shell_print_colored("                 ║\n", ANSI_CYAN);
        shell_print_colored("║ ", ANSI_CYAN);
        shell_print_colored("  help   - Show this help message   ║\n", ANSI_CYAN);
        shell_print_colored("║ ", ANSI_CYAN);
        shell_print_colored("  clear  - Clear the screen         ║\n", ANSI_CYAN);
        shell_print_colored("║ ", ANSI_CYAN);
        shell_print_colored("  pwd    - Print working directory  ║\n", ANSI_CYAN);
        shell_print_colored("║ ", ANSI_CYAN);
        shell_print_colored("  cd     - Change directory         ║\n", ANSI_CYAN);
        shell_print_colored("║ ", ANSI_CYAN);
        shell_print_colored("  reboot - Reboot the system        ║\n", ANSI_CYAN);
        shell_print_colored("║ ", ANSI_CYAN);
        shell_print_colored("  gui    - Start GUI demo          ║\n", ANSI_CYAN);
        shell_print_colored("║ ", ANSI_CYAN);
        shell_print_colored("Other commands are executed via ELF.║\n", ANSI_CYAN);
        shell_print_colored("╚═════════════════════════════════════╝\n", ANSI_CYAN);
    } else if (!strcmp(cmd, "clear")) {
        flanterm_write(ft_ctx, "\033[2J\033[H", 7); // ANSI clear + home
    } else if (!strcmp(cmd, "reboot")) {
        // Trigger reboot via ACPI or similar mechanism
        // For now, we use the keyboard controller method (works on QEMU)
        asm volatile ("outb %%al, $0x64" :: "a"(0xFE));
        shell_print("Reboot command sent.\n");
        // Halt if reboot doesn't happen immediately
        for (;;) { asm volatile ("cli; hlt"); }
    } else if (!strcmp(cmd, "gui")) {
        gui_draw_desktop(&gui_ctx);
        for (;;) { asm volatile("cli; hlt"); }
    } else if (!strcmp(cmd, "pwd")) {
        // Print working directory
        const char *cwd = fs_get_current_dir();
        shell_print(ANSI_GREEN);
        shell_print(cwd);
        shell_print(ANSI_RESET "\n");
    } else if (!strcmp(cmd, "cd")) {
        // Change directory
        if (argc < 2) {
            // Change to root directory if no argument
            if (!fs_change_dir("/")) {
                 shell_print(ANSI_RED "Error: " ANSI_RESET "Could not change to root directory\n");
            }
        } else {
            if (!fs_change_dir(argv[1])) {
                shell_print(ANSI_RED "Error: " ANSI_RESET "Could not change directory to ");
                shell_print(argv[1]);
                shell_print("\n");
            }
        }
        // No output on success, following Unix convention
    }
    // --- External Commands (ELF Execution) --- 
    else {
        if (!try_exec_elf_command(cmd, argv, argc)) {
            // If execution wasn't attempted (file not found)
            shell_print(ANSI_RED "Error: " ANSI_RESET "Command not found: ");
            shell_print(cmd);
            shell_print("\n");
        }
        // If try_exec_elf_command returned true, the process ran (or failed during exec_elf)
        // and exec_elf handled cleanup. Nothing more to do here.
    }
}

// Main shell loop
void shell_run(void) {
    static char buffer[SHELL_BUFSZ];
    static size_t buf_idx = 0;
    static char *argv[SHELL_MAX_ARGS];
    static int argc = 0;
    static char arg_bufs[SHELL_MAX_ARGS][SHELL_MAX_ARG_LEN];

    shell_print_colored("\nLimine Kernel Shell\n", ANSI_BOLD ANSI_CYAN);
    shell_print("Type 'help' for available commands.\n");

    while (true) {
        // Print prompt
        const char *cwd = fs_get_current_dir();
        shell_print_colored("user@limine", ANSI_BOLD ANSI_GREEN);
        shell_print(":");
        shell_print_colored(cwd, ANSI_BOLD ANSI_BLUE);
        shell_print("$ ");
        flanterm_flush(ft_ctx);

        // Reset buffer and args for new command
        memset(buffer, 0, SHELL_BUFSZ);
        buf_idx = 0;
        argc = 0;

        // Read command line
        while (buf_idx < SHELL_BUFSZ - 1) {
            char c = keyboard_read_char();
            if (!c) continue; // Skip if no character available

            if (c == '\n' || c == '\r') {
                flanterm_write(ft_ctx, "\n", 1);
                break; // End of command
            } else if (c == '\b' || c == 127) { // Backspace or DEL
                if (buf_idx > 0) {
                    buf_idx--;
                    buffer[buf_idx] = 0;
                    flanterm_write(ft_ctx, "\b \b", 3); // Erase character on screen
                }
            } else if (c >= 32 && c < 127) { // Printable ASCII
                buffer[buf_idx++] = c;
                flanterm_write(ft_ctx, &c, 1); // Echo character
            }
            flanterm_flush(ft_ctx); // Flush after each character for responsiveness
        }
        buffer[buf_idx] = 0; // Null-terminate the command line

        // --- Parse command line into argv --- 
        argc = 0;
        char *token = strtok(buffer, " \t\n\r"); // Use strtok for simple parsing
        while (token != NULL && argc < SHELL_MAX_ARGS) {
            // Copy token into persistent storage
            strncpy(arg_bufs[argc], token, SHELL_MAX_ARG_LEN - 1);
            arg_bufs[argc][SHELL_MAX_ARG_LEN - 1] = 0; // Ensure null termination
            argv[argc] = arg_bufs[argc];
            argc++;
            token = strtok(NULL, " \t\n\r");
        }
        argv[argc] = NULL; // Null-terminate argv array

        // --- Execute command --- 
        if (argc > 0) {
            shell_exec(argv[0], argv, argc);
        }
    }
}

