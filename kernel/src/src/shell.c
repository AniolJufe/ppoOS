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

static const char *user_list[] = {"user", "sudo", NULL};
static const char *current_user = "user";

// ----------------- Environment Variable Support -----------------
#define SHELL_MAX_ENV_VARS   32
#define SHELL_MAX_ENV_NAME   32
#define SHELL_MAX_ENV_VALUE 128

struct env_var {
    char name[SHELL_MAX_ENV_NAME];
    char value[SHELL_MAX_ENV_VALUE];
};

static struct env_var env_vars[SHELL_MAX_ENV_VARS];
static bool shell_exit_requested = false;

static const char *get_env_var(const char *name) {
    for (int i = 0; i < SHELL_MAX_ENV_VARS; i++) {
        if (env_vars[i].name[0] && !strcmp(env_vars[i].name, name)) {
            return env_vars[i].value;
        }
    }
    return NULL;
}

static bool set_env_var(const char *name, const char *value) {
    for (int i = 0; i < SHELL_MAX_ENV_VARS; i++) {
        if (env_vars[i].name[0] && !strcmp(env_vars[i].name, name)) {
            strncpy(env_vars[i].value, value, SHELL_MAX_ENV_VALUE - 1);
            env_vars[i].value[SHELL_MAX_ENV_VALUE - 1] = 0;
            return true;
        }
    }
    for (int i = 0; i < SHELL_MAX_ENV_VARS; i++) {
        if (!env_vars[i].name[0]) {
            strncpy(env_vars[i].name, name, SHELL_MAX_ENV_NAME - 1);
            env_vars[i].name[SHELL_MAX_ENV_NAME - 1] = 0;
            strncpy(env_vars[i].value, value, SHELL_MAX_ENV_VALUE - 1);
            env_vars[i].value[SHELL_MAX_ENV_VALUE - 1] = 0;
            return true;
        }
    }
    return false; // No space
}

static void unset_env_var(const char *name) {
    for (int i = 0; i < SHELL_MAX_ENV_VARS; i++) {
        if (env_vars[i].name[0] && !strcmp(env_vars[i].name, name)) {
            env_vars[i].name[0] = 0;
            env_vars[i].value[0] = 0;
        }
    }
}

static void init_default_env(void) {
    set_env_var("PATH", "/bin:/usr/bin:." );
    set_env_var("USER", current_user);
}

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

static unsigned short parse_octal(const char *s) {
    unsigned short v = 0;
    while (*s >= '0' && *s <= '7') {
        v = (v << 3) | (*s - '0');
        s++;
    }
    return v;
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
    char path_buffer[256];

    const char *path_env = get_env_var("PATH");
    const char *paths = path_env ? path_env : "/bin:/usr/bin:.";
    char paths_copy[256];
    strncpy(paths_copy, paths, sizeof(paths_copy) - 1);
    paths_copy[sizeof(paths_copy) - 1] = 0;

    char *tok = strtok(paths_copy, ":");
    while (tok) {
        strncpy(path_buffer, tok, sizeof(path_buffer) - 1);
        path_buffer[sizeof(path_buffer) - 1] = 0;
        size_t len = strlen(path_buffer);
        if (len && path_buffer[len - 1] != '/')
            strncat(path_buffer, "/", sizeof(path_buffer) - len - 1);
        strncat(path_buffer, cmd, sizeof(path_buffer) - strlen(path_buffer) - 1);

        struct fs_file *file = fs_open(path_buffer);
        if (file != NULL) {
            (void)argv;
            (void)argc;

            shell_print("Executing: ");
            shell_print(path_buffer);
            shell_print("\n");

            exec_elf(path_buffer);
            return true;
        }
        tok = strtok(NULL, ":");
    }

    return false;
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
        shell_print_colored("  ls     - List files              ║\n", ANSI_CYAN);
        shell_print_colored("  chmod  - Change file mode        ║\n", ANSI_CYAN);
        shell_print_colored("  export - Set env variable        ║\n", ANSI_CYAN);
        shell_print_colored("  unset  - Remove env variable     ║\n", ANSI_CYAN);
        shell_print_colored("  set    - List env variables      ║\n", ANSI_CYAN);
        shell_print_colored("  exit   - Leave shell             ║\n", ANSI_CYAN);
        shell_print_colored("║ ", ANSI_CYAN);
        shell_print_colored("  su     - Switch user             ║\n", ANSI_CYAN);

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
        gui_run_demo(&gui_ctx);
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
    } else if (!strcmp(cmd, "ls")) {
        // List files in the current directory
        size_t n = 0;
        const struct fs_file *files = fs_list(&n);
        for (size_t i = 0; i < n; i++) {
            shell_print(files[i].name);
            if (files[i].is_dir) shell_print("/");
            shell_print("  ");
        }
        shell_print("\n");

    } else if (!strcmp(cmd, "chmod")) {
        if (argc < 3) {
            shell_print("Usage: chmod <mode> <file>\n");
        } else {
            unsigned short mode = parse_octal(argv[1]);
            if (!fs_chmod(argv[2], mode)) {
                shell_print("chmod: failed to change mode\n");
            }
        }
    } else if (!strcmp(cmd, "export")) {
        if (argc < 3) {
            shell_print("Usage: export <name> <value>\n");
        } else {
            set_env_var(argv[1], argv[2]);
        }
    } else if (!strcmp(cmd, "unset")) {
        if (argc < 2) {
            shell_print("Usage: unset <name>\n");
        } else {
            unset_env_var(argv[1]);
        }
    } else if (!strcmp(cmd, "set")) {
        for (int i = 0; i < SHELL_MAX_ENV_VARS; i++) {
            if (env_vars[i].name[0]) {
                shell_print(env_vars[i].name);
                shell_print("=");
                shell_print(env_vars[i].value);
                shell_print("\n");
            }
        }
    } else if (!strcmp(cmd, "exit")) {
        shell_exit_requested = true;
        return;
    } else if (!strcmp(cmd, "su")) {
        if (argc < 2) {
            shell_print("Usage: su <user>\n");
        } else {
            for (int i = 0; user_list[i]; i++) {
                if (!strcmp(argv[1], user_list[i])) {
                    current_user = user_list[i];
                    set_env_var("USER", current_user);
                    return;
                }
            }
            shell_print("Unknown user\n");
        }

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

    init_default_env();
    while (!shell_exit_requested) {
        // Print prompt
        const char *cwd = fs_get_current_dir();
        shell_print_colored(current_user, ANSI_BOLD ANSI_GREEN);
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
        bool in_quotes = false;
        size_t arg_idx = 0;
        for (size_t i = 0; i <= buf_idx && argc < SHELL_MAX_ARGS; i++) {
            char c = buffer[i];
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if ((c == ' ' || c == '\t' || c == 0) && !in_quotes) {
                if (arg_idx > 0 || c == 0) {
                    arg_bufs[argc][arg_idx] = 0;
                    // Variable expansion for tokens starting with '$'
                    if (arg_bufs[argc][0] == '$') {
                        const char *val = get_env_var(&arg_bufs[argc][1]);
                        if (val)
                            strncpy(arg_bufs[argc], val, SHELL_MAX_ARG_LEN - 1);
                        arg_bufs[argc][SHELL_MAX_ARG_LEN - 1] = 0;
                    }
                    argv[argc] = arg_bufs[argc];
                    argc++;
                    arg_idx = 0;
                }
            } else {
                if (arg_idx < SHELL_MAX_ARG_LEN - 1)
                    arg_bufs[argc][arg_idx++] = c;
            }
        }
        argv[argc] = NULL; // Null-terminate argv array

        // --- Execute command --- 
        if (argc > 0) {
            shell_exec(argv[0], argv, argc);
        }
    }
}

