#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>

#include "src/kernel.h"
#include "src/serial.h"
#include "src/vmm.h"
#include "src/idt.h"
#include "src/gdt.h"
#include "src/syscall.h"
#include "lib/string_test.h" // For test_ultoa_hex

// Set the base revision to 2, this is recommended as this is the latest
// base revision described by the Limine boot protocol specification.
// See specification for further info.

__attribute__((used, section(".requests")))
static volatile LIMINE_BASE_REVISION(2);

// The Limine requests can be placed anywhere, but it is important that
// the compiler does not optimise them away, so, usually, they should
// be made volatile or equivalent, _and_ they should be accessed at least
// once or marked as used with the "used" attribute as done here.

__attribute__((used, section(".requests")))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used, section(".requests")))
volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

// Request the memory map.
__attribute__((used, section(".requests")))
volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

// Request the kernel's physical and virtual address.
__attribute__((used, section(".requests")))
volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0
};

// Request the HHDM offset
__attribute__((used, section(".requests")))
volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

// Finally, define the start and end markers for the Limine requests.
// These can also be moved anywhere, to any .c file, as seen fit.

__attribute__((used, section(".requests_start_marker")))
static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".requests_end_marker")))
static volatile LIMINE_REQUESTS_END_MARKER;

// GCC and Clang reserve the right to generate calls to the following
// 4 functions even if they are not directly called.
// Implement them as the C specification mandates.
// DO NOT remove or rename these functions, or stuff will eventually break!
// They CAN be moved to a different .c file.


// Halt and catch fire function.
static void hcf(void) {
    for (;;) {
#if defined (__x86_64__)
        asm ("hlt");
#elif defined (__aarch64__) || defined (__riscv)
        asm ("wfi");
#elif defined (__loongarch64)
        asm ("idle 0");
#endif
    }
}

bool checkStringEndsWith(const char* str, const char* end)
{
    const char* _str = str;
    const char* _end = end;

    while(*str != 0)
        str++;
    str--;

    while(*end != 0)
        end++;
    end--;

    while (true)
    {
        if (*str != *end)
            return false;

        str--;
        end--;

        if (end == _end || (str == _str && end == _end))
            return true;

        if (str == _str)
            return false;
    }

    return true;
}

struct limine_file* getFile(const char* name)
{
    struct limine_module_response *module_response = module_request.response;

    if (module_response == NULL)
    {
        hcf();
    }

    for (size_t i = 0; i < module_response->module_count; i++) 
    {
        struct limine_file *f = module_response->modules[i];
        // Extract just the filename from the path
        const char *filename = f->path;
        const char *last_slash = filename;
        while (*filename) {
            if (*filename == '/' || *filename == '\\') {
                last_slash = filename + 1;
            }
            filename++;
        }
        if (checkStringEndsWith(last_slash, name))
            return f;
    }
    
    return NULL;
}

// The following will be our kernel's entry point.
// If renaming kmain() to something else, make sure to change the
// linker script accordingly.
void kmain(void)
{
    // Ensure the framebuffer model is supported.
    if (framebuffer_request.response == NULL || framebuffer_request.response->framebuffer_count < 1) {
        hcf();
    }

    // Initialise serial first for debugging
    serial_init();
    serial_write("Kernel started.\n", 15);

    // Run library tests
    test_ultoa_hex();
    serial_write("string_test completed.\n", 23);


    // Initialize Physical Memory Manager
    pmm_init();

    // Initialize GDT and TSS (must be before IDT)
    gdt_init();

    // Initialize Interrupt Descriptor Table (IDT)
    idt_init();

    // Initialize Syscall MSRs (must be after GDT/IDT)
    syscall_init();

    // Fetch the first framebuffer.
    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    struct Framebuffer fb_struct = {
        .base_address = fb->address,
        .width = fb->width,
        .height = fb->height,
        .pixels_per_scan_line = fb->pitch / 4
    };

    kernel(fb_struct);
    hcf();
}
