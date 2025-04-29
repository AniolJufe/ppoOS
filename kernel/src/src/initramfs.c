#include "initramfs.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Only supports newc format, minimal parser
#define CPIO_NEWC_MAGIC "070701"

static const void *ramfs_base = NULL;
static size_t ramfs_len = 0;

static struct initramfs_file files[16];
static size_t file_count = 0;

static uint32_t hex2u32(const char *s, size_t n) {
    uint32_t v = 0;
    while (n--) {
        v <<= 4;
        if (*s >= '0' && *s <= '9') v |= *s - '0';
        else if (*s >= 'a' && *s <= 'f') v |= *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') v |= *s - 'A' + 10;
        s++;
    }
    return v;
}

void initramfs_init(const void *base, size_t len) {
    ramfs_base = base;
    ramfs_len = len;
    file_count = 0;
    const uint8_t *p = (const uint8_t *)base;
    const uint8_t *end = p + len;

    while (p + 110 < end && file_count < 16) {
        // Check header magic
        if (memcmp(p, CPIO_NEWC_MAGIC, 6) != 0) {
            // Optional: Print error or break differently if magic fails mid-archive
            break;
        }

        // Read sizes from header
        uint32_t namesize = hex2u32((const char *)(p + 94), 8);
        uint32_t filesize = hex2u32((const char *)(p + 54), 8);

        // Calculate name start
        const char *name = (const char *)(p + 110);

        // Check for trailer entry BEFORE storing
        if (namesize > 0 && strcmp(name, "TRAILER!!!") == 0) {
            break;
        }

        // Calculate file data start, considering padding after name
        uintptr_t name_end_unpadded = (uintptr_t)p + 110 + namesize;
        uintptr_t filedata_start_aligned = (name_end_unpadded + 3) & ~3;
        const void *filedata = (const void *)filedata_start_aligned;

        // Calculate next header start, considering padding after file data
        uintptr_t filedata_end_unpadded = filedata_start_aligned + filesize;
        uintptr_t next_header_start_aligned = (filedata_end_unpadded + 3) & ~3;

        // Check if calculated pointers are within bounds
        if ((const uint8_t *)next_header_start_aligned > end) {
            // Optional: Print error about archive corruption/truncation
            break;
        }

        // Store file info (already checked it's not the trailer)
        files[file_count].name = name;
        files[file_count].data = filedata;
        files[file_count].size = filesize;
        file_count++;

        // Advance pointer to the start of the next header
        p = (const uint8_t *)next_header_start_aligned;
    }
}

const struct initramfs_file *initramfs_find(const char *name) {
    for (size_t i = 0; i < file_count; ++i) {
        if (strcmp(files[i].name, name) == 0) return &files[i];
    }
    return NULL;
}

// Enumerate files by index
const struct initramfs_file *initramfs_list(size_t idx) {
    if (idx < file_count) return &files[idx];
    return NULL;
}
