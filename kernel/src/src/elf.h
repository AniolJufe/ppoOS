#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ELF Header Magic bytes
#define ELF_MAGIC 0x464C457F  // \x7FELF in little endian

// ELF file class (32/64 bit)
#define ELFCLASS32 1
#define ELFCLASS64 2

// ELF data encoding
#define ELFDATA2LSB 1  // Little endian
#define ELFDATA2MSB 2  // Big endian

// ELF file types
#define ET_NONE 0      // No file type
#define ET_REL  1      // Relocatable file
#define ET_EXEC 2      // Executable file
#define ET_DYN  3      // Shared object file
#define ET_CORE 4      // Core file

// ELF machine types (architecture)
#define EM_386    3   // Intel 80386
#define EM_ARM    40  // ARM
#define EM_X86_64 62  // AMD x86-64
#define EM_AARCH64 183 // ARM 64-bits

// Section types
#define SHT_NULL     0  // Null section
#define SHT_PROGBITS 1  // Program data
#define SHT_SYMTAB   2  // Symbol table
#define SHT_STRTAB   3  // String table
#define SHT_RELA     4  // Relocation entries with addends
#define SHT_HASH     5  // Symbol hash table
#define SHT_DYNAMIC  6  // Dynamic linking information
#define SHT_NOTE     7  // Notes
#define SHT_NOBITS   8  // Program space with no data (bss)
#define SHT_REL      9  // Relocation entries, no addends
#define SHT_SHLIB    10 // Reserved
#define SHT_DYNSYM   11 // Dynamic linker symbol table

// Section flags
#define SHF_WRITE     0x1        // Writable
#define SHF_ALLOC     0x2        // Occupies memory during execution
#define SHF_EXECINSTR 0x4        // Executable

// Program header types
#define PT_NULL    0  // Unused
#define PT_LOAD    1  // Loadable program segment
#define PT_DYNAMIC 2  // Dynamic linking information
#define PT_INTERP  3  // Program interpreter
#define PT_NOTE    4  // Auxiliary information
#define PT_SHLIB   5  // Reserved
#define PT_PHDR    6  // Entry for header table itself

// Program header flags
#define PF_X        0x1        // Executable
#define PF_W        0x2        // Writable
#define PF_R        0x4        // Readable

// ELF Header (common part for 32/64 bit)
typedef struct {
    uint32_t e_magic;       // Must be 0x7F 'E' 'L' 'F'
    uint8_t  e_class;       // 1=32bit, 2=64bit
    uint8_t  e_data;        // 1=little endian, 2=big endian
    uint8_t  e_version;     // Current version (1)
    uint8_t  e_osabi;       // OS ABI identification
    uint8_t  e_abiversion;  // ABI version
    uint8_t  e_pad[7];      // Padding bytes
    uint16_t e_type;        // Object file type
    uint16_t e_machine;     // Architecture
    uint32_t e_version2;    // Object file version
} __attribute__((packed)) elf_common_header_t;

// 32-bit ELF header (extends common part)
typedef struct {
    elf_common_header_t common;
    uint32_t e_entry;       // Entry point virtual address
    uint32_t e_phoff;       // Program header table file offset
    uint32_t e_shoff;       // Section header table file offset
    uint32_t e_flags;       // Processor-specific flags
    uint16_t e_ehsize;      // ELF header size in bytes
    uint16_t e_phentsize;   // Program header table entry size
    uint16_t e_phnum;       // Program header table entry count
    uint16_t e_shentsize;   // Section header table entry size
    uint16_t e_shnum;       // Section header table entry count
    uint16_t e_shstrndx;    // Section header string table index
} __attribute__((packed)) elf32_header_t;

// 64-bit ELF header (extends common part)
typedef struct {
    elf_common_header_t common;
    uint64_t e_entry;       // Entry point virtual address
    uint64_t e_phoff;       // Program header table file offset
    uint64_t e_shoff;       // Section header table file offset
    uint32_t e_flags;       // Processor-specific flags
    uint16_t e_ehsize;      // ELF header size in bytes
    uint16_t e_phentsize;   // Program header table entry size
    uint16_t e_phnum;       // Program header table entry count
    uint16_t e_shentsize;   // Section header table entry size
    uint16_t e_shnum;       // Section header table entry count
    uint16_t e_shstrndx;    // Section header string table index
} __attribute__((packed)) elf64_header_t;

// 32-bit Program header
typedef struct {
    uint32_t p_type;        // Segment type
    uint32_t p_offset;      // Segment file offset
    uint32_t p_vaddr;       // Segment virtual address
    uint32_t p_paddr;       // Segment physical address
    uint32_t p_filesz;      // Segment size in file
    uint32_t p_memsz;       // Segment size in memory
    uint32_t p_flags;       // Segment flags
    uint32_t p_align;       // Segment alignment
} __attribute__((packed)) elf32_program_header_t;

// 64-bit Program header
typedef struct {
    uint32_t p_type;        // Segment type
    uint32_t p_flags;       // Segment flags
    uint64_t p_offset;      // Segment file offset
    uint64_t p_vaddr;       // Segment virtual address
    uint64_t p_paddr;       // Segment physical address
    uint64_t p_filesz;      // Segment size in file
    uint64_t p_memsz;       // Segment size in memory
    uint64_t p_align;       // Segment alignment
} __attribute__((packed)) elf64_program_header_t;

// 32-bit Section header
typedef struct {
    uint32_t sh_name;       // Section name (string table index)
    uint32_t sh_type;       // Section type
    uint32_t sh_flags;      // Section flags
    uint32_t sh_addr;       // Section virtual address
    uint32_t sh_offset;     // Section file offset
    uint32_t sh_size;       // Section size in bytes
    uint32_t sh_link;       // Link to another section
    uint32_t sh_info;       // Additional section information
    uint32_t sh_addralign;  // Section alignment
    uint32_t sh_entsize;    // Entry size if section holds table
} __attribute__((packed)) elf32_section_header_t;

// 64-bit Section header
typedef struct {
    uint32_t sh_name;       // Section name (string table index)
    uint32_t sh_type;       // Section type
    uint64_t sh_flags;      // Section flags
    uint64_t sh_addr;       // Section virtual address
    uint64_t sh_offset;     // Section file offset
    uint64_t sh_size;       // Section size in bytes
    uint32_t sh_link;       // Link to another section
    uint32_t sh_info;       // Additional section information
    uint64_t sh_addralign;  // Section alignment
    uint64_t sh_entsize;    // Entry size if section holds table
} __attribute__((packed)) elf64_section_header_t;

// Standard ELF32 Program Header
typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} elf32_phdr_t;

// Standard ELF64 Program Header
typedef struct {
    uint32_t p_type;    // Segment type
    uint32_t p_flags;   // Segment flags
    uint64_t p_offset;  // Segment file offset
    uint64_t p_vaddr;   // Segment virtual address
    uint64_t p_paddr;   // Segment physical address (optional)
    uint64_t p_filesz;  // Segment size in file
    uint64_t p_memsz;   // Segment size in memory
    uint64_t p_align;   // Segment alignment
} elf64_phdr_t;

// Program header types (p_type)
#define PT_NULL    0
#define PT_LOAD    1
#define PT_DYNAMIC 2
#define PT_INTERP  3
#define PT_NOTE    4
#define PT_SHLIB   5
#define PT_PHDR    6
#define PT_TLS     7

// Standard Auxiliary Vector entry structure (ELF64)
typedef struct {
    uint64_t a_type; // Entry type
    union {
        uint64_t a_val; // Entry value
    } a_un;
} Elf64_auxv_t;

// Auxiliary Vector types (a_type)
// Note: These values must match standard Linux/System V ABI values
#define AT_NULL   0      // End of vector
#define AT_IGNORE 1      // Ignore entry
#define AT_EXECFD 2      // File descriptor of program
#define AT_PHDR   3      // Program headers for program
#define AT_PHENT  4      // Size of program header entry
#define AT_PHNUM  5      // Number of program headers
#define AT_PAGESZ 6      // System page size
#define AT_BASE   7      // Base address of interpreter
#define AT_FLAGS  8      // Flags
#define AT_ENTRY  9      // Entry point of program
#define AT_NOTELF 10     // Program is not ELF
#define AT_UID    11     // Real uid
#define AT_EUID   12     // Effective uid
#define AT_GID    13     // Real gid
#define AT_EGID   14     // Effective gid
#define AT_PLATFORM 15   // String identifying platform
#define AT_HWCAP  16     // Machine-dependent hints about
                         // processor capabilities.
#define AT_CLKTCK 17     // Frequency of times()
#define AT_SECURE 23     // Boolean, was exec setuid/setgid?
#define AT_BASE_PLATFORM 24 // String identifying real platform, may
                            // differ from AT_PLATFORM.
#define AT_RANDOM 25     // Address of 16 random bytes.
#define AT_HWCAP2 26     // More machine-dependent hints about
                         // processor capabilities.
#define AT_EXECFN 31     // Filename of executable.

// Functions for parsing and displaying ELF information
bool elf_is_valid(const void* elf_data, size_t size);
bool elf_is_64bit(const void* elf_data);
void elf_print_info(const void* elf_data, size_t size);
