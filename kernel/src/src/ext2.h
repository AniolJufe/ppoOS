#ifndef EXT2_H
#define EXT2_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "fs.h"

// EXT2 Superblock structure
struct ext2_superblock {
    uint32_t inodes_count;
    uint32_t blocks_count;
    uint32_t r_blocks_count;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t first_data_block;
    uint32_t log_block_size;
    uint32_t log_frag_size;
    uint32_t blocks_per_group;
    uint32_t frags_per_group;
    uint32_t inodes_per_group;
    uint32_t mtime;
    uint32_t wtime;
    uint16_t mnt_count;
    uint16_t max_mnt_count;
    uint16_t magic;  // Should be 0xEF53
    uint16_t state;
    uint16_t errors;
    uint16_t minor_rev_level;
    uint32_t lastcheck;
    uint32_t checkinterval;
    uint32_t creator_os;
    uint32_t rev_level;
    uint16_t def_resuid;
    uint16_t def_resgid;
    // EXT2 revision 1 specific fields
    uint32_t first_ino;
    uint16_t inode_size;
    uint16_t block_group_nr;
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;
    uint8_t uuid[16];
    char volume_name[16];
    char last_mounted[64];
    uint32_t algo_bitmap;
    // Performance hints
    uint8_t prealloc_blocks;
    uint8_t prealloc_dir_blocks;
    uint16_t padding1;
    // Journaling support
    uint8_t journal_uuid[16];
    uint32_t journal_inum;
    uint32_t journal_dev;
    uint32_t last_orphan;
    // Directory indexing support
    uint32_t hash_seed[4];
    uint8_t def_hash_version;
    uint8_t padding2[3];
    // Other options
    uint32_t default_mount_options;
    uint32_t first_meta_bg;
    uint8_t reserved[760]; // Padding to 1024 bytes
} __attribute__((packed));

// EXT2 Block Group Descriptor
struct ext2_group_desc {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint8_t reserved[12];
} __attribute__((packed));

// EXT2 Inode structure
struct ext2_inode {
    uint16_t mode;
    uint16_t uid;
    uint32_t size;
    uint32_t atime;
    uint32_t ctime;
    uint32_t mtime;
    uint32_t dtime;
    uint16_t gid;
    uint16_t links_count;
    uint32_t blocks;
    uint32_t flags;
    uint32_t osd1;
    uint32_t block[15]; // Direct, indirect, double indirect, triple indirect
    uint32_t generation;
    uint32_t file_acl;
    uint32_t dir_acl;
    uint32_t faddr;
    uint8_t osd2[12];
} __attribute__((packed));

// EXT2 Directory Entry
struct ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t name_len;
    uint8_t file_type;
    char name[];
} __attribute__((packed));

// EXT2 file types
#define EXT2_FT_UNKNOWN  0
#define EXT2_FT_REG_FILE 1
#define EXT2_FT_DIR      2
#define EXT2_FT_CHRDEV   3
#define EXT2_FT_BLKDEV   4
#define EXT2_FT_FIFO     5
#define EXT2_FT_SOCK     6
#define EXT2_FT_SYMLINK  7

// EXT2 file mode bits
#define EXT2_S_IFMT   0xF000  // Format mask
#define EXT2_S_IFSOCK 0xC000  // Socket
#define EXT2_S_IFLNK  0xA000  // Symbolic link
#define EXT2_S_IFREG  0x8000  // Regular file
#define EXT2_S_IFBLK  0x6000  // Block device
#define EXT2_S_IFDIR  0x4000  // Directory
#define EXT2_S_IFCHR  0x2000  // Character device
#define EXT2_S_IFIFO  0x1000  // FIFO

// EXT2 magic number
#define EXT2_SUPER_MAGIC 0xEF53

// EXT2 driver functions
bool ext2_detect(const void *data, size_t size);
bool ext2_init(const void *data, size_t size);
struct fs_file *ext2_open(const char *path);
size_t ext2_read(const struct fs_file *file, size_t offset, void *buf, size_t len);
const struct fs_file *ext2_list(const char *path, size_t *count);
bool ext2_change_dir(const char *path);

#endif // EXT2_H