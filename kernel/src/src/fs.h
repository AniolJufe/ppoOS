#ifndef FS_H
#define FS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define FS_MAX_PATH 128
#define FS_MAX_FILES 32
#define FS_MAX_DIRS 8

// Filesystem types
typedef enum {
    FS_TYPE_INITRAMFS = 0,
    FS_TYPE_EXT2 = 1,
    FS_TYPE_UNKNOWN = 255
} fs_type_t;

// File entry with mutable fields
struct fs_file {
    char name[32];      // Filename with null terminator
    char *data;         // File content (mutable)
    size_t size;        // File size
    size_t capacity;    // Allocated capacity for data
    bool is_dir;        // Whether this entry is a directory
    fs_type_t fs_type;  // Which filesystem this file belongs to
};

// Directory entry
struct fs_dir {
    char name[32];          // Directory name
    char path[FS_MAX_PATH]; // Full path
};

// File system mount
struct fs_mount {
    struct fs_file files[FS_MAX_FILES];
    size_t file_count;
    struct fs_dir dirs[FS_MAX_DIRS];
    size_t dir_count;
    char current_dir[FS_MAX_PATH];
    fs_type_t active_fs;    // Currently active filesystem
};

// Initialize filesystem
void fs_init(void);

// Initialize ext2 filesystem from a disk image
bool fs_mount_ext2(const void *data, size_t size);

// Get current directory
const char *fs_get_current_dir(void);

// Change directory (returns true if successful)
bool fs_change_dir(const char *path);

// Open a file by name (returns NULL if not found)
struct fs_file *fs_open(const char *name);

// Read from file (returns number of bytes read)
size_t fs_read(const struct fs_file *file, size_t offset, void *buf, size_t len);

// List files in current directory (returns pointer to array and count)
const struct fs_file *fs_list(size_t *count);

// Create a new empty file (returns the new file or NULL if failed)
struct fs_file *fs_create_file(const char *name);

// Write to a file (returns number of bytes written)
size_t fs_write(struct fs_file *file, size_t offset, const void *buf, size_t len);

// Create a new directory
bool fs_create_dir(const char *name);

#endif // FS_H
