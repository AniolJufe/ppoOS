#include "ext2.h"
#include "serial.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// EXT2 driver state
static const void *ext2_data = NULL;
static size_t ext2_size = 0;
static struct ext2_superblock *superblock = NULL;
static uint32_t block_size = 0;
static char current_dir[FS_MAX_PATH] = "/";

// Temporary storage for file listings
static struct fs_file file_cache[FS_MAX_FILES];
static size_t file_cache_count = 0;

// Helper function to read a block from the ext2 filesystem
static void *ext2_read_block(uint32_t block_num) {
    if (!ext2_data || !block_size || block_num >= superblock->blocks_count) {
        return NULL;
    }
    
    // Calculate the offset of the block
    size_t offset = block_num * block_size;
    if (offset + block_size > ext2_size) {
        return NULL; // Out of bounds
    }
    
    return (void *)((uint8_t *)ext2_data + offset);
}

// Helper function to read an inode from the ext2 filesystem
static struct ext2_inode *ext2_read_inode(uint32_t inode_num) {
    if (!ext2_data || !superblock || inode_num == 0 || 
        inode_num > superblock->inodes_count) {
        return NULL;
    }
    
    // Adjust inode number (inodes start at 1, not 0)
    inode_num--;
    
    // Calculate which block group the inode is in
    uint32_t inodes_per_group = superblock->inodes_per_group;
    uint32_t group = inode_num / inodes_per_group;
    uint32_t index = inode_num % inodes_per_group;
    
    // Read the block group descriptor
    uint32_t bg_desc_offset = (superblock->first_data_block + 1) * block_size;
    struct ext2_group_desc *bg_desc = (struct ext2_group_desc *)
        ((uint8_t *)ext2_data + bg_desc_offset + (group * sizeof(struct ext2_group_desc)));
    
    // Calculate the offset of the inode table
    uint32_t inode_table_block = bg_desc->inode_table;
    uint32_t inode_size = superblock->inode_size;
    uint32_t inode_offset = (inode_table_block * block_size) + (index * inode_size);
    
    if (inode_offset + inode_size > ext2_size) {
        return NULL; // Out of bounds
    }
    
    return (struct ext2_inode *)((uint8_t *)ext2_data + inode_offset);
}

// Helper function to read data from an inode
static size_t ext2_read_inode_data(struct ext2_inode *inode, size_t offset, 
                                  void *buf, size_t len) {
    if (!inode || !buf || offset >= inode->size) {
        return 0;
    }
    
    // Limit read length to file size
    if (offset + len > inode->size) {
        len = inode->size - offset;
    }
    
    // Calculate which block to start reading from
    uint32_t start_block = offset / block_size;
    uint32_t end_block = (offset + len - 1) / block_size;
    uint32_t block_offset = offset % block_size;
    size_t bytes_read = 0;
    
    // Read direct blocks (0-11)
    for (uint32_t i = start_block; i <= end_block && i < 12; i++) {
        void *block_data = ext2_read_block(inode->block[i]);
        if (!block_data) {
            break;
        }
        
        size_t to_read = block_size;
        if (i == start_block) {
            // First block may be partial
            to_read -= block_offset;
            block_data = (uint8_t *)block_data + block_offset;
        }
        if (i == end_block) {
            // Last block may be partial
            size_t end_offset = (offset + len) % block_size;
            if (end_offset > 0) {
                to_read = end_offset - (i == start_block ? block_offset : 0);
            }
        }
        
        // Don't read more than requested
        if (bytes_read + to_read > len) {
            to_read = len - bytes_read;
        }
        
        memcpy((uint8_t *)buf + bytes_read, block_data, to_read);
        bytes_read += to_read;
        
        if (bytes_read >= len) {
            break;
        }
    }
    
    // TODO: Implement indirect block reading if needed
    // This simplified implementation only handles direct blocks
    
    return bytes_read;
}

// Helper function to find an inode by path
static struct ext2_inode *ext2_find_inode_by_path(const char *path) {
    if (!path || !*path) {
        return NULL;
    }
    
    // Start from root inode (inode 2 in ext2)
    struct ext2_inode *current_inode = ext2_read_inode(2);
    if (!current_inode) {
        return NULL;
    }
    
    // Handle root directory case
    if (strcmp(path, "/") == 0) {
        return current_inode;
    }
    
    // Make a copy of the path to tokenize
    char path_copy[FS_MAX_PATH];
    strncpy(path_copy, path, FS_MAX_PATH - 1);
    path_copy[FS_MAX_PATH - 1] = '\0';
    
    // Skip leading slash
    char *token = path_copy;
    if (*token == '/') {
        token++;
    }
    
    // Tokenize the path and traverse the directory structure
    char *next_token = NULL;
    while (token && *token) {
        // Find the next path component
        next_token = strchr(token, '/');
        if (next_token) {
            *next_token = '\0';
            next_token++;
        }
        
        // Skip empty components
        if (!*token) {
            token = next_token;
            continue;
        }
        
        // Make sure current inode is a directory
        if (!(current_inode->mode & EXT2_S_IFDIR)) {
            return NULL; // Not a directory
        }
        
        // Search for the token in the current directory
        bool found = false;
        uint8_t block_buffer[block_size];
        
        // Iterate through directory blocks
        for (int i = 0; i < 12 && !found; i++) {
            if (current_inode->block[i] == 0) {
                continue;
            }
            
            void *block_data = ext2_read_block(current_inode->block[i]);
            if (!block_data) {
                continue;
            }
            
            // Copy block data to our buffer
            memcpy(block_buffer, block_data, block_size);
            
            // Iterate through directory entries
            size_t offset = 0;
            while (offset < block_size) {
                struct ext2_dir_entry *entry = 
                    (struct ext2_dir_entry *)(block_buffer + offset);
                
                if (entry->inode == 0 || entry->rec_len == 0) {
                    break;
                }
                
                // Compare entry name with token
                if (entry->name_len == strlen(token) && 
                    strncmp(entry->name, token, entry->name_len) == 0) {
                    // Found the entry, read its inode
                    current_inode = ext2_read_inode(entry->inode);
                    if (!current_inode) {
                        return NULL;
                    }
                    found = true;
                    break;
                }
                
                // Move to next entry
                offset += entry->rec_len;
            }
        }
        
        if (!found) {
            return NULL; // Path component not found
        }
        
        // Move to next token
        token = next_token;
    }
    
    return current_inode;
}

// Detect if the given data is an ext2 filesystem
bool ext2_detect(const void *data, size_t size) {
    if (!data || size < 1024 + sizeof(struct ext2_superblock)) {
        return false;
    }
    
    // The superblock starts at offset 1024
    struct ext2_superblock *sb = (struct ext2_superblock *)((uint8_t *)data + 1024);
    
    // Check the magic number
    return sb->magic == EXT2_SUPER_MAGIC;
}

// Initialize the ext2 driver with the given data
bool ext2_init(const void *data, size_t size) {
    if (!ext2_detect(data, size)) {
        return false;
    }
    
    ext2_data = data;
    ext2_size = size;
    
    // Read the superblock
    superblock = (struct ext2_superblock *)((uint8_t *)data + 1024);
    
    // Calculate block size (1024 << log_block_size)
    block_size = 1024 << superblock->log_block_size;
    
    // Initialize current directory
    strcpy(current_dir, "/");
    
    serial_write("[ext2] Filesystem mounted successfully\n", 38);
    serial_write("[ext2] Block size: ", 18);
    
    // Convert block size to string
    char size_str[16] = {0};
    size_t size_val = block_size;
    size_t idx = 0;
    if (size_val == 0) {
        size_str[idx++] = '0';
    } else {
        // Convert to string in reverse
        while (size_val > 0) {
            size_str[idx++] = '0' + (size_val % 10);
            size_val /= 10;
        }
        // Reverse the string
        for (size_t i = 0; i < idx / 2; i++) {
            char tmp = size_str[i];
            size_str[i] = size_str[idx - i - 1];
            size_str[idx - i - 1] = tmp;
        }
    }
    size_str[idx] = '\0';
    
    serial_write(size_str, strlen(size_str));
    serial_write(" bytes\n", 7);
    
    return true;
}

// Open a file by path
struct fs_file *ext2_open(const char *path) {
    if (!path || !*path) {
        return NULL;
    }
    
    // Find the inode for the given path
    struct ext2_inode *inode = ext2_find_inode_by_path(path);
    if (!inode) {
        return NULL;
    }
    
    // Create a fs_file structure for the inode
    static struct fs_file file;
    memset(&file, 0, sizeof(file));
    
    // Extract the filename from the path
    const char *filename = path;
    const char *last_slash = strrchr(path, '/');
    if (last_slash) {
        filename = last_slash + 1;
    }
    
    // Set file properties
    strncpy(file.name, filename, sizeof(file.name) - 1);
    file.name[sizeof(file.name) - 1] = '\0';
    file.size = inode->size;
    file.is_dir = (inode->mode & EXT2_S_IFDIR) != 0;
    
    // Store the inode number in the data pointer (hack for this simple implementation)
    // In a real implementation, we would allocate a proper structure
    file.data = (char *)(uintptr_t)inode->blocks;
    file.capacity = inode->size;
    
    return &file;
}

// Read from a file
size_t ext2_read(const struct fs_file *file, size_t offset, void *buf, size_t len) {
    if (!file || !buf || offset >= file->size) {
        return 0;
    }
    
    // Get the inode number from the data pointer
    uint32_t inode_blocks = (uint32_t)(uintptr_t)file->data;
    
    // Find the inode for the file
    struct ext2_inode *inode = NULL;
    
    // This is a simplified approach - in a real implementation, we would store
    // the inode number directly and look it up
    for (uint32_t i = 1; i <= superblock->inodes_count; i++) {
        struct ext2_inode *temp_inode = ext2_read_inode(i);
        if (temp_inode && temp_inode->blocks == inode_blocks) {
            inode = temp_inode;
            break;
        }
    }
    
    if (!inode) {
        return 0;
    }
    
    // Read the data from the inode
    return ext2_read_inode_data(inode, offset, buf, len);
}

// List files in a directory
const struct fs_file *ext2_list(const char *path, size_t *count) {
    if (!path) {
        path = current_dir;
    }
    
    // Find the inode for the given path
    struct ext2_inode *dir_inode = ext2_find_inode_by_path(path);
    if (!dir_inode || !(dir_inode->mode & EXT2_S_IFDIR)) {
        if (count) {
            *count = 0;
        }
        return NULL;
    }
    
    // Clear the file cache
    file_cache_count = 0;
    
    // Iterate through directory blocks
    for (int i = 0; i < 12 && file_cache_count < FS_MAX_FILES; i++) {
        if (dir_inode->block[i] == 0) {
            continue;
        }
        
        void *block_data = ext2_read_block(dir_inode->block[i]);
        if (!block_data) {
            continue;
        }
        
        // Iterate through directory entries
        size_t offset = 0;
        while (offset < block_size && file_cache_count < FS_MAX_FILES) {
            struct ext2_dir_entry *entry = 
                (struct ext2_dir_entry *)((uint8_t *)block_data + offset);
            
            if (entry->inode == 0 || entry->rec_len == 0) {
                break;
            }
            
            // Skip "." and ".." entries
            if (!(entry->name_len == 1 && entry->name[0] == '.') &&
                !(entry->name_len == 2 && entry->name[0] == '.' && entry->name[1] == '.')) {
                
                // Read the inode for this entry
                struct ext2_inode *inode = ext2_read_inode(entry->inode);
                if (inode) {
                    // Add to file cache
                    struct fs_file *file = &file_cache[file_cache_count++];
                    memset(file, 0, sizeof(struct fs_file));
                    
                    // Copy the name (ensure null termination)
                    memcpy(file->name, entry->name, entry->name_len);
                    file->name[entry->name_len] = '\0';
                    
                    // Set file properties
                    file->size = inode->size;
                    file->is_dir = (inode->mode & EXT2_S_IFDIR) != 0;
                    
                    // Store the inode number in the data pointer
                    file->data = (char *)(uintptr_t)inode->blocks;
                    file->capacity = inode->size;
                }
            }
            
            // Move to next entry
            offset += entry->rec_len;
        }
    }
    
    if (count) {
        *count = file_cache_count;
    }
    
    return file_cache;
}

// Change current directory
bool ext2_change_dir(const char *path) {
    if (!path || !*path) {
        return false;
    }
    
    // Find the inode for the given path
    struct ext2_inode *inode = ext2_find_inode_by_path(path);
    if (!inode || !(inode->mode & EXT2_S_IFDIR)) {
        return false;
    }
    
    // Update current directory
    if (path[0] == '/') {
        // Absolute path
        strncpy(current_dir, path, FS_MAX_PATH - 1);
    } else {
        // Relative path
        size_t current_len = strlen(current_dir);
        if (current_dir[current_len - 1] != '/') {
            strncat(current_dir, "/", FS_MAX_PATH - current_len - 1);
            current_len++;
        }
        strncat(current_dir, path, FS_MAX_PATH - current_len - 1);
    }
    
    // Ensure null termination
    current_dir[FS_MAX_PATH - 1] = '\0';
    
    return true;
}