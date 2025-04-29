#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct Framebuffer {
    void *base_address;
    size_t buffer_size;
    unsigned int width;
    unsigned int height;
    unsigned int pixels_per_scan_line;
};

// Forward declaration for flanterm context
struct flanterm_context;

void kernel(struct Framebuffer fb);
