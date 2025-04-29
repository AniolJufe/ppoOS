#include "elf.h"
#include <stddef.h>
#include <string.h>

// ELF utility functions for shell commands

// Check if a file is a valid ELF file
bool elf_is_valid(const void* elf_data, size_t size) {
    if (size < sizeof(elf_common_header_t)) {
        return false;
    }
    
    const elf_common_header_t* header = (const elf_common_header_t*)elf_data;
    return header->e_magic == ELF_MAGIC && 
           (header->e_class == ELFCLASS32 || header->e_class == ELFCLASS64) &&
           (header->e_data == ELFDATA2LSB || header->e_data == ELFDATA2MSB) &&
           header->e_version == 1;
}

// Check if an ELF file is 64-bit
bool elf_is_64bit(const void* elf_data) {
    const elf_common_header_t* header = (const elf_common_header_t*)elf_data;
    return header->e_class == ELFCLASS64;
}

// Get ELF type as a string
static const char* elf_get_type_str(uint16_t e_type) {
    switch (e_type) {
        case ET_NONE: return "NONE (No file type)";
        case ET_REL:  return "REL (Relocatable file)";
        case ET_EXEC: return "EXEC (Executable file)";
        case ET_DYN:  return "DYN (Shared object file)";
        case ET_CORE: return "CORE (Core file)";
        default:      return "UNKNOWN";
    }
}

// Get ELF machine type as a string
static const char* elf_get_machine_str(uint16_t e_machine) {
    switch (e_machine) {
        case EM_386:     return "Intel 80386";
        case EM_ARM:     return "ARM";
        case EM_X86_64:  return "AMD x86-64";
        case EM_AARCH64: return "ARM 64-bits";
        default:         return "Unknown Machine";
    }
}

// Get Section Type as string
static const char* elf_get_section_type_str(uint32_t sh_type) {
    switch (sh_type) {
        case SHT_NULL:     return "NULL";
        case SHT_PROGBITS: return "PROGBITS";
        case SHT_SYMTAB:   return "SYMTAB";
        case SHT_STRTAB:   return "STRTAB";
        case SHT_RELA:     return "RELA";
        case SHT_HASH:     return "HASH";
        case SHT_DYNAMIC:  return "DYNAMIC";
        case SHT_NOTE:     return "NOTE";
        case SHT_NOBITS:   return "NOBITS";
        case SHT_REL:      return "REL";
        case SHT_SHLIB:    return "SHLIB";
        case SHT_DYNSYM:   return "DYNSYM";
        default:           return "UNKNOWN";
    }
}

// Our own string concatenation function
static void str_append(char* dest, const char* src) {
    // Find the end of dest
    while (*dest) dest++;
    
    // Copy src to the end of dest
    while (*src) {
        *dest = *src;
        dest++;
        src++;
    }
    
    // Null terminate
    *dest = '\0';
}

// Generate section flags string
static void elf_get_section_flags_str(char* buf, size_t size, uint64_t sh_flags) {
    memset(buf, 0, size);
    
    if (sh_flags & SHF_WRITE)     str_append(buf, "W");
    if (sh_flags & SHF_ALLOC)     str_append(buf, "A");
    if (sh_flags & SHF_EXECINSTR) str_append(buf, "X");
}

// Get Program Type as string
static const char* elf_get_program_type_str(uint32_t p_type) {
    switch (p_type) {
        case PT_NULL:    return "NULL";
        case PT_LOAD:    return "LOAD";
        case PT_DYNAMIC: return "DYNAMIC";
        case PT_INTERP:  return "INTERP";
        case PT_NOTE:    return "NOTE";
        case PT_SHLIB:   return "SHLIB";
        case PT_PHDR:    return "PHDR";
        default:         return "UNKNOWN";
    }
}

// Generate program flags string
static void elf_get_program_flags_str(char* buf, size_t size, uint32_t p_flags) {
    memset(buf, 0, size);
    
    if (p_flags & PF_R) str_append(buf, "R");
    if (p_flags & PF_W) str_append(buf, "W");
    if (p_flags & PF_X) str_append(buf, "X");
}

// Custom atoi function (stdlib not available)
static int my_atoi(const char* str) {
    int result = 0;
    int sign = 1;
    
    // Skip leading spaces
    while (*str == ' ') str++;
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert to integer
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}

// Custom simple itoa function
static void my_itoa(int n, char* buffer, int base) {
    int i = 0;
    int is_negative = 0;
    
    // Handle 0 explicitly
    if (n == 0) {
        buffer[i++] = '0';
        buffer[i] = '\0';
        return;
    }
    
    // Handle negative numbers (for base 10)
    if (n < 0 && base == 10) {
        is_negative = 1;
        n = -n;
    }
    
    // Process digits
    while (n != 0) {
        int remainder = n % base;
        buffer[i++] = (remainder < 10) ? (remainder + '0') : (remainder + 'a' - 10);
        n = n / base;
    }
    
    // Add negative sign if needed
    if (is_negative) {
        buffer[i++] = '-';
    }
    
    // Null terminate
    buffer[i] = '\0';
    
    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }
}

// Custom hex converter
static void my_itohex(unsigned int n, char* buffer) {
    char hex_chars[] = "0123456789abcdef";
    int i = 0;
    
    // Handle 0 explicitly
    if (n == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    // Process hex digits
    while (n != 0) {
        buffer[i++] = hex_chars[n % 16];
        n = n / 16;
    }
    
    // Add 0x prefix
    buffer[i++] = 'x';
    buffer[i++] = '0';
    
    // Null terminate
    buffer[i] = '\0';
    
    // Reverse the string
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = temp;
        start++;
        end--;
    }
}

// Custom string formatting function (simple printf replacement)
// Function pointer for output
typedef void (*print_function_t)(const char*);
static print_function_t print_func = NULL;

// Set the print function
void elf_set_print_function(print_function_t func) {
    print_func = func;
}

// Print a string
static void elf_print(const char* str) {
    if (print_func) {
        print_func(str);
    }
}

// Print ELF header information with simplified output
static void elf_print_header(const void* elf_data) {
    // Print common header fields
    const elf_common_header_t* common = (const elf_common_header_t*)elf_data;
    char buffer[64];
    
    elf_print("ELF Header:\n");
    
    // Magic
    elf_print("  Magic:   0x7F ELF\n");
    
    // Class
    elf_print("  Class:   ");
    elf_print(common->e_class == ELFCLASS32 ? "ELF32" : "ELF64");
    elf_print("\n");
    
    // Data
    elf_print("  Data:    ");
    elf_print(common->e_data == ELFDATA2LSB ? "2's complement, little endian" : "2's complement, big endian");
    elf_print("\n");
    
    // Version
    elf_print("  Version: 1 (current)\n");
    
    // Type
    elf_print("  Type:    ");
    elf_print(elf_get_type_str(common->e_type));
    elf_print("\n");
    
    // Machine
    elf_print("  Machine: ");
    elf_print(elf_get_machine_str(common->e_machine));
    elf_print("\n");
    
    if (common->e_class == ELFCLASS32) {
        // 32-bit specific fields
        const elf32_header_t* header = (const elf32_header_t*)elf_data;
        
        // Number of program headers
        elf_print("  Program headers: ");
        my_itoa(header->e_phnum, buffer, 10);
        elf_print(buffer);
        elf_print("\n");
        
        // Number of section headers
        elf_print("  Section headers: ");
        my_itoa(header->e_shnum, buffer, 10);
        elf_print(buffer);
        elf_print("\n");
    } else {
        // 64-bit specific fields
        const elf64_header_t* header = (const elf64_header_t*)elf_data;
        
        // Number of program headers
        elf_print("  Program headers: ");
        my_itoa(header->e_phnum, buffer, 10);
        elf_print(buffer);
        elf_print("\n");
        
        // Number of section headers
        elf_print("  Section headers: ");
        my_itoa(header->e_shnum, buffer, 10);
        elf_print(buffer);
        elf_print("\n");
    }
}

// Print ELF program headers (simplified)
static void elf_print_program_headers(const void* elf_data) {
    char buffer[64];
    
    elf_print("\nProgram Headers:\n");
    
    if (elf_is_64bit(elf_data)) {
        const elf64_header_t* header = (const elf64_header_t*)elf_data;
        
        // Skip if no program headers
        if (header->e_phnum == 0) {
            elf_print("  No program headers\n");
            return;
        }
        
        elf_print("  Type            Flags   Offset     VirtAddr\n");
        
        for (int i = 0; i < header->e_phnum && i < 10; i++) { // Limit to 10 entries
            const elf64_program_header_t* ph = (const elf64_program_header_t*)((uint8_t*)elf_data + header->e_phoff + i * header->e_phentsize);
            
            // Type
            elf_print("  ");
            elf_print(elf_get_program_type_str(ph->p_type));
            
            // Padding
            elf_print("           ");
            
            // Flags
            char flags[4] = "";
            elf_get_program_flags_str(flags, sizeof(flags), ph->p_flags);
            elf_print(flags);
            elf_print("    ");
            
            // Virtual address
            elf_print("0x");
            my_itoa((int)(ph->p_vaddr & 0xFFFFFFFF), buffer, 16);
            elf_print(buffer);
            elf_print("\n");
        }
    } else {
        const elf32_header_t* header = (const elf32_header_t*)elf_data;
        
        // Skip if no program headers
        if (header->e_phnum == 0) {
            elf_print("  No program headers\n");
            return;
        }
        
        elf_print("  Type            Flags   Offset     VirtAddr\n");
        
        for (int i = 0; i < header->e_phnum && i < 10; i++) { // Limit to 10 entries
            const elf32_program_header_t* ph = (const elf32_program_header_t*)((uint8_t*)elf_data + header->e_phoff + i * header->e_phentsize);
            
            // Type
            elf_print("  ");
            elf_print(elf_get_program_type_str(ph->p_type));
            
            // Padding
            elf_print("           ");
            
            // Flags
            char flags[4] = "";
            elf_get_program_flags_str(flags, sizeof(flags), ph->p_flags);
            elf_print(flags);
            elf_print("    ");
            
            // Virtual address
            elf_print("0x");
            my_itoa(ph->p_vaddr, buffer, 16);
            elf_print(buffer);
            elf_print("\n");
        }
    }
}

// Print ELF section headers (simplified)
static void elf_print_section_headers(const void* elf_data) {
    char buffer[64];
    
    elf_print("\nSection Headers:\n");
    
    if (elf_is_64bit(elf_data)) {
        const elf64_header_t* header = (const elf64_header_t*)elf_data;
        
        // Skip if no section headers
        if (header->e_shnum == 0) {
            elf_print("  No section headers\n");
            return;
        }
        
        elf_print("  Type            Flags   Address    Size\n");
        
        for (int i = 0; i < header->e_shnum && i < 15; i++) { // Limit to 15 entries
            const elf64_section_header_t* sh = (const elf64_section_header_t*)((uint8_t*)elf_data + header->e_shoff + i * header->e_shentsize);
            
            // Type
            elf_print("  ");
            elf_print(elf_get_section_type_str(sh->sh_type));
            
            // Padding
            elf_print("           ");
            
            // Flags
            char flags[4] = "";
            elf_get_section_flags_str(flags, sizeof(flags), sh->sh_flags);
            elf_print(flags);
            elf_print("    ");
            
            // Address
            elf_print("0x");
            my_itoa((int)(sh->sh_addr & 0xFFFFFFFF), buffer, 16);
            elf_print(buffer);
            elf_print("  ");
            
            // Size
            elf_print("0x");
            my_itoa((int)(sh->sh_size & 0xFFFFFFFF), buffer, 16);
            elf_print(buffer);
            elf_print("\n");
        }
    } else {
        const elf32_header_t* header = (const elf32_header_t*)elf_data;
        
        // Skip if no section headers
        if (header->e_shnum == 0) {
            elf_print("  No section headers\n");
            return;
        }
        
        elf_print("  Type            Flags   Address    Size\n");
        
        for (int i = 0; i < header->e_shnum && i < 15; i++) { // Limit to 15 entries
            const elf32_section_header_t* sh = (const elf32_section_header_t*)((uint8_t*)elf_data + header->e_shoff + i * header->e_shentsize);
            
            // Type
            elf_print("  ");
            elf_print(elf_get_section_type_str(sh->sh_type));
            
            // Padding
            elf_print("           ");
            
            // Flags
            char flags[4] = "";
            elf_get_section_flags_str(flags, sizeof(flags), sh->sh_flags);
            elf_print(flags);
            elf_print("    ");
            
            // Address
            elf_print("0x");
            my_itoa(sh->sh_addr, buffer, 16);
            elf_print(buffer);
            elf_print("  ");
            
            // Size
            elf_print("0x");
            my_itoa(sh->sh_size, buffer, 16);
            elf_print(buffer);
            elf_print("\n");
        }
    }
}

// Main function to print all ELF information
void elf_print_info(const void* elf_data, size_t size) {
    if (!elf_is_valid(elf_data, size)) {
        elf_print("Not a valid ELF file\n");
        return;
    }
    
    elf_print_header(elf_data);
    elf_print_program_headers(elf_data);
    elf_print_section_headers(elf_data);
}
