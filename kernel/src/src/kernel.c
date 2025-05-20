#include "kernel.h"
#include <stddef.h>
#include <stdint.h>
#include <flanterm.h>
#include <backends/fb.h>
#include "keyboard.h"
#include "shell.h"
#include "initramfs.h"
#include "fs.h"
#include <limine.h>
#include <string.h>
#include "serial.h"
#include "syscall.h"
#include "vmm.h"
#include "gui.h"

struct flanterm_context *ft_ctx;
static struct gui_context gui_ctx;

#define SHELL_BUFSZ 256

// Reference to the module request defined in main.c
extern volatile struct limine_module_request module_request;

void kernel(struct Framebuffer framebuffer) {
    serial_init();

    // Initialize Memory Management (PMM and VMM)
    pmm_init();
    vmm_init(); // Initialize VMM and store kernel PML4

    // Initialize CPU syscall MSRs (EFER, STAR, LSTAR, FMASK)
    cpu_init();
    ft_ctx = flanterm_fb_init(
        NULL, NULL,
        (uint32_t*)framebuffer.base_address,
        framebuffer.width,
        framebuffer.height,
        framebuffer.pixels_per_scan_line * 4,
        8, 16, // red mask size/shift (example, may need adjustment)
        8, 8,  // green mask size/shift
        8, 0,  // blue mask size/shift
        NULL, // canvas
        NULL, NULL, // ansi_colours, ansi_bright_colours
        NULL, NULL, // default_bg, default_fg
        NULL, NULL, // default_bg_bright, default_fg_bright
        NULL, 0, 0, 1, // font, font_width, font_height, font_spacing
        0, 0, // font_scale_x, font_scale_y
        0 // margin
    );
    gui_init(&gui_ctx, framebuffer);
    gui_draw_desktop(&gui_ctx);
    syscall_init();
    const char msg[] = "Welcome to limine-shell (flanterm)!\n";
    flanterm_write(ft_ctx, msg, sizeof(msg)-1);
    serial_write(msg, sizeof(msg)-1);

    // Initramfs: search for the initramfs module by path
    struct limine_module_response *mod_resp = module_request.response;
    const void *mod_base = NULL;
    size_t mod_len = 0;
    if (mod_resp && mod_resp->module_count > 0) {
        flanterm_write(ft_ctx, "[initramfs: found modules: ", 26);
        serial_write("[initramfs: found modules: ", 26);
        for (uint64_t i = 0; i < mod_resp->module_count; i++) {
            struct limine_file *mod = mod_resp->modules[i];
            // Print module path for debugging
            if (mod->path) {
                flanterm_write(ft_ctx, mod->path, strlen(mod->path));
                serial_write(mod->path, strlen(mod->path));
            } else {
                flanterm_write(ft_ctx, "(null path)", 11);
                serial_write("(null path)", 11);
            }
            flanterm_write(ft_ctx, " ", 1);
            serial_write(" ", 1);
            // Extract just the filename from the path
            const char *path = mod->path;
            const char *last_slash = path;
            
            if (path) {
                while (*path) {
                    if (*path == '/' || *path == '\\') {
                        last_slash = path + 1;
                    }
                    path++;
                }
                
                // Check if the filename is initramfs.cpio (case-sensitive)
                if (strcmp(last_slash, "initramfs.cpio") == 0) {
                    mod_base = mod->address;
                    mod_len = mod->size;
                    break;
                }
            }
        }
        flanterm_write(ft_ctx, "]\n", 2);
        serial_write("]\n", 2);
        if (mod_base && mod_len > 0) {
            initramfs_init(mod_base, mod_len);
            fs_init();
            // List files at boot
            size_t nfiles = 0;
            const struct fs_file *files = fs_list(&nfiles);
            flanterm_write(ft_ctx, "[initramfs: files: ", 18);
            serial_write("[initramfs: files: ", 18);
            for (size_t i = 0; i < nfiles; ++i) {
                flanterm_write(ft_ctx, files[i].name, strlen(files[i].name));
                serial_write(files[i].name, strlen(files[i].name));
                if (i < nfiles - 1)
                    flanterm_write(ft_ctx, ", ", 2);
                    serial_write(", ", 2);
            }
            flanterm_write(ft_ctx, "]\n", 2);
            serial_write("]\n", 2);
        } else {
            flanterm_write(ft_ctx, "[initramfs: no module found by path]\n", 34);
            serial_write("[initramfs: no module found by path]\n", 34);
        }
    } else {
        flanterm_write(ft_ctx, "[initramfs: no module response or count is zero]\n", 48);
        serial_write("[initramfs: no module response or count is zero]\n", 48);
    }

    shell_run();
}
