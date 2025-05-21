#include "fs.h"
#include "initramfs.h"
#include "ext2.h"
#include "serial.h"
#include <stddef.h>
#include <string.h>

// File system instance
static struct fs_mount fs;

// Memory allocation helper
static void *fs_alloc(size_t size) {
    // This is a very simple allocator for our demo
    // In a real system, we'd use a proper memory allocator
    static char mem_pool[64 * 1024]; // 64KB memory pool
    static size_t mem_used = 0;
    
    if (mem_used + size > sizeof(mem_pool)) {
        return NULL; // Out of memory
    }
    
    void *ptr = mem_pool + mem_used;
    mem_used += size;
    return ptr;
}

void fs_init(void) {
    // Clear filesystem state
    memset(&fs, 0, sizeof(fs));
    
    // Set root directory
    strcpy(fs.current_dir, "/");
    
    // Create root directory
    struct fs_dir *root = &fs.dirs[fs.dir_count++];
    strcpy(root->name, "/");
    strcpy(root->path, "/");
    
    // Set default filesystem type to initramfs
    fs.active_fs = FS_TYPE_INITRAMFS;
    
    // Import files from initramfs
    size_t count = 0;
    while (1) {
        const struct initramfs_file *src = initramfs_list(count);
        if (!src) break;
        count++;
        
        if (fs.file_count >= FS_MAX_FILES) {
            serial_write("[fs_init] Warning: Reached max files limit\n", 41);
            break; // No more room
        }
        
        size_t name_len = strlen(src->name);
        if (name_len > 0 && src->name[name_len - 1] == '/') {
            char dir_name[32];
            if (name_len - 1 >= sizeof(dir_name))
                name_len = sizeof(dir_name) - 1;
            memcpy(dir_name, src->name, name_len - 1);
            dir_name[name_len - 1] = '\0';
            fs_create_dir(dir_name);
            continue;
        }

        struct fs_file *dst = &fs.files[fs.file_count++];
        strncpy(dst->name, src->name, sizeof(dst->name) - 1);
        dst->name[sizeof(dst->name) - 1] = '\0';

        dst->capacity = src->size;
        dst->data = fs_alloc(dst->capacity);
        if (!dst->data) {
            fs.file_count--;
            break;
        }

        memcpy(dst->data, src->data, src->size);
        dst->size = src->size;
        dst->is_dir = false;
        dst->mode = 0644;
        dst->fs_type = FS_TYPE_INITRAMFS;
    }
    
    serial_write("[fs_init] Initialized filesystem with ", 38);
    // Convert file count to string
    char count_str[8] = {0};
    size_t count_val = fs.file_count;
    size_t idx = 0;
    if (count_val == 0) {
        count_str[idx++] = '0';
    } else {
        // Convert to string in reverse
        while (count_val > 0) {
            count_str[idx++] = '0' + (count_val % 10);
            count_val /= 10;
        }
        // Reverse the string
        for (size_t i = 0; i < idx / 2; i++) {
            char tmp = count_str[i];
            count_str[i] = count_str[idx - i - 1];
            count_str[idx - i - 1] = tmp;
        }
    }
    count_str[idx] = '\0';
    
    serial_write(count_str, strlen(count_str));
    serial_write(" files\n", 7);
    
    // Check if any of the files is an ext2 image that we can mount
    for (size_t i = 0; i < fs.file_count; i++) {
        struct fs_file *file = &fs.files[i];
        if (strcmp(file->name, "ext2.img") == 0 || 
            strcmp(file->name, "disk.img") == 0) {
            // Try to mount as ext2
            if (ext2_detect(file->data, file->size)) {
                serial_write("[fs_init] Found ext2 image: ", 27);
                serial_write(file->name, strlen(file->name));
                serial_write("\n", 1);
                if (fs_mount_ext2(file->data, file->size)) {
                    serial_write("[fs_init] Successfully mounted ext2 filesystem\n", 45);
                    break;
                }
            }
        }
    }
}

// Mount an ext2 filesystem
bool fs_mount_ext2(const void *data, size_t size) {
    if (!ext2_init(data, size)) {
        return false;
    }
    
    // Switch to ext2 filesystem
    fs.active_fs = FS_TYPE_EXT2;
    return true;
}

const char *fs_get_current_dir(void) {
    return fs.current_dir;
}

bool fs_change_dir(const char *path) {
    // Handle special cases
    if (!path || !*path) {
        return false;
    }
    
    // Handle root directory
    if (strcmp(path, "/") == 0) {
        strcpy(fs.current_dir, "/");
        return true;
    }
    
    // Handle based on active filesystem
    if (fs.active_fs == FS_TYPE_EXT2) {
        // Use ext2 driver
        if (ext2_change_dir(path)) {
            // Update our current directory
            strcpy(fs.current_dir, path);
            return true;
        }
        return false;
    } else {
        // Use initramfs driver (original implementation)
        for (size_t i = 0; i < fs.dir_count; i++) {
            if (strcmp(fs.dirs[i].name, path) == 0 || 
                strcmp(fs.dirs[i].path, path) == 0) {
                strcpy(fs.current_dir, fs.dirs[i].path);
                return true;
            }
        }
        return false;
    }
}

struct fs_file *fs_open(const char *name) {
    if (!name || !*name) return NULL;


    // Handle based on active filesystem
    if (fs.active_fs == FS_TYPE_EXT2) {
        // Use ext2 driver
        return ext2_open(name);
    }
    
    // Use initramfs driver (original implementation)
    // Create buffers for comparison - initialize to zeros to prevent garbage data
    char processed_name[32] = {0};
    char processed_entry[32] = {0};

    // Strip leading './' from the requested name if present
    if (strncmp(name, "./", 2) == 0) {
        strncpy(processed_name, name + 2, sizeof(processed_name) - 1);
    } else {
        strncpy(processed_name, name, sizeof(processed_name) - 1);
    }
    processed_name[sizeof(processed_name) - 1] = '\0'; // Ensure null termination


    // Search for the file in our registry
    for (size_t i = 0; i < fs.file_count; i++) {
        // Clear the entry buffer for each iteration
        memset(processed_entry, 0, sizeof(processed_entry));
        
        // Make a clean copy of the entry name
        strncpy(processed_entry, fs.files[i].name, sizeof(processed_entry) - 1);
        processed_entry[sizeof(processed_entry) - 1] = '\0';

        // Strip './' from the entry name if present
        if (strncmp(processed_entry, "./", 2) == 0) {
            // Safer approach - copy the string starting at position 2
            for (size_t j = 0; j < sizeof(processed_entry) - 3; j++) {
                processed_entry[j] = processed_entry[j + 2];
            }
            // Ensure string remains null-terminated
            processed_entry[sizeof(processed_entry) - 3] = '\0';
        }


        // Try both the processed name and direct comparison
        if (strcmp(processed_entry, processed_name) == 0 || 
            strcmp(fs.files[i].name, processed_name) == 0 ||
            strcmp(fs.files[i].name, name) == 0) {
            return &fs.files[i];
        }
    }

    return NULL;
}

size_t fs_read(const struct fs_file *file, size_t offset, void *buf, size_t len) {
    if (!file || offset >= file->size) return 0;
    
    // Handle based on file's filesystem type
    if (file->fs_type == FS_TYPE_EXT2) {
        // Use ext2 driver
        return ext2_read(file, offset, buf, len);
    } else {
        // Use initramfs driver (original implementation)
        if (!file->data) return 0;
        
        size_t to_copy = file->size - offset;
        if (to_copy > len) to_copy = len;
        
        memcpy(buf, file->data + offset, to_copy);
        return to_copy;
    }
}

const struct fs_file *fs_list(size_t *count) {
    // Handle based on active filesystem
    if (fs.active_fs == FS_TYPE_EXT2) {
        // Use ext2 driver
        return ext2_list(fs.current_dir, count);
    } else {
        // Use initramfs driver (original implementation)
        if (count) {
            *count = fs.file_count;
        }
        return fs.files;
    }
}

struct fs_file *fs_create_file(const char *name) {
    // Check if already exists
    struct fs_file *existing = fs_open(name);
    if (existing) {
        return existing; // File already exists
    }
    
    // For now, only support file creation in initramfs
    if (fs.active_fs == FS_TYPE_EXT2) {
        serial_write("[fs_create_file] File creation not supported in ext2 yet\n", 55);
        return NULL;
    }
    
    // Check if we have room for a new file
    if (fs.file_count >= FS_MAX_FILES) {
        return NULL; // No more room
    }
    
    // Create new file
    struct fs_file *file = &fs.files[fs.file_count++];
    strncpy(file->name, name, sizeof(file->name) - 1);
    file->name[sizeof(file->name) - 1] = '\0';
    
    // Allocate initial buffer
    file->capacity = 256; // Start with 256 bytes
    file->data = fs_alloc(file->capacity);
    if (!file->data) {
        fs.file_count--; // Revert the file addition
        return NULL;
    }
    
    file->size = 0;
    file->data[0] = '\0'; // Empty string
    file->is_dir = false;
    file->mode = 0644;
    file->fs_type = FS_TYPE_INITRAMFS;
    
    return file;
}

size_t fs_write(struct fs_file *file, size_t offset, const void *buf, size_t len) {
    if (!file) return 0;
    
    // For now, only support writing to initramfs files
    if (file->fs_type == FS_TYPE_EXT2) {
        serial_write("[fs_write] Writing to ext2 files not supported yet\n", 49);
        return 0;
    }
    
    if (!file->data) return 0;
    
    // Check if we need to expand the buffer
    size_t new_size = offset + len;
    if (new_size > file->capacity) {
        // Calculate new capacity (double the needed size)
        size_t new_capacity = new_size * 2;
        
        // Allocate new buffer
        char *new_data = fs_alloc(new_capacity);
        if (!new_data) {
            return 0; // Failed to allocate
        }
        
        // Zero the new buffer so unwritten regions don't expose stale data
        memset(new_data, 0, new_capacity);

        // Copy existing file contents
        memcpy(new_data, file->data, file->size);
        
        // Update file struct
        file->data = new_data;
        file->capacity = new_capacity;
    }
    
    // If writing past the current end, zero the gap so reads return consistent data
    if (offset > file->size) {
        memset(file->data + file->size, 0, offset - file->size);
    }

    // Write the data
    memcpy(file->data + offset, buf, len);
    
    // Update size if needed
    if (new_size > file->size) {
        file->size = new_size;
        file->data[file->size] = '\0'; // Ensure null termination
    }
    
    return len;
}

bool fs_create_dir(const char *name) {
    // For now, only support directory creation in initramfs
    if (fs.active_fs == FS_TYPE_EXT2) {
        serial_write("[fs_create_dir] Directory creation not supported in ext2 yet\n", 60);
        return false;
    }
    
    // Check if already exists
    for (size_t i = 0; i < fs.dir_count; i++) {
        if (strcmp(fs.dirs[i].name, name) == 0) {
            return true; // Directory already exists
        }
    }
    
    // Check if we have room for a new directory
    if (fs.dir_count >= FS_MAX_DIRS) {
        return false; // No more room
    }
    
    // Create new directory
    struct fs_dir *dir = &fs.dirs[fs.dir_count++];
    strncpy(dir->name, name, sizeof(dir->name) - 1);
    dir->name[sizeof(dir->name) - 1] = '\0';
    
    // Set the full path safely, avoiding snprintf
    size_t max_len = sizeof(dir->path);
    size_t name_len = strlen(name);
    if (strcmp(fs.current_dir, "/") == 0) {
        // Root directory case: path = "/" + name
        if (1 + name_len + 1 > max_len) { // Check length: '/' + name + null terminator
            fs.dir_count--; // Revert dir creation
            return false; // Path too long
        }
        strcpy(dir->path, "/");
        strcat(dir->path, name);
    } else {
        // Subdirectory case: path = current_dir + "/" + name
        size_t current_len = strlen(fs.current_dir);
        if (current_len + 1 + name_len + 1 > max_len) { // Check length: current + '/' + name + null
            fs.dir_count--; // Revert dir creation
            return false; // Path too long
        }
        strcpy(dir->path, fs.current_dir);
        strcat(dir->path, "/");
        strcat(dir->path, name);
    }

    // Ensure null termination just in case (should be handled by strcpy/strcat)
    dir->path[max_len - 1] = '\0';

    // Also create an entry in the file list so directory shows up in listings
    if (fs.file_count < FS_MAX_FILES) {
        struct fs_file *f = &fs.files[fs.file_count++];
        memset(f, 0, sizeof(*f));
        strncpy(f->name, name, sizeof(f->name) - 1);
        f->name[sizeof(f->name) - 1] = '\0';
        f->is_dir = true;
        f->fs_type = FS_TYPE_INITRAMFS;
    }

    return true;
}

bool fs_chmod(const char *name, unsigned short mode) {
    struct fs_file *f = fs_open(name);
    if (!f) {
        return false;
    }
    f->mode = mode;
    return true;
}
