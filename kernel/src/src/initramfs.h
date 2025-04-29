#ifndef INITRAMFS_H
#define INITRAMFS_H

#include <stddef.h>
#include <stdint.h>

struct initramfs_file {
    const char *name;
    const void *data;
    size_t size;
};

// Returns pointer to file data, or NULL if not found
const struct initramfs_file *initramfs_find(const char *name);
// Enumerate files by index (returns NULL when done)
const struct initramfs_file *initramfs_list(size_t idx);
// Call this once at boot, passing module base and length from Limine
void initramfs_init(const void *base, size_t len);

#endif // INITRAMFS_H
