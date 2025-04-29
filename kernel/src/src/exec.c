#include "exec.h"
#include "lib/string.h"
#include "usermode_entry.h" // For jmp_usermode
#include "vmm.h"          
// #include "pmm.h" // REMOVED - Prototypes are in vmm.h
// #include "filesystem.h" // For getFile and struct limine_file
#include "serial.h"
#include "fs.h" // Include our filesystem header
// Use specific local elf.h if available, otherwise rely on system includes
#include "elf.h"     // Use local elf.h
#include <limine.h>  // For struct limine_file (if not in filesystem.h)
#include <stdint.h>
#include <stddef.h>

// Forward declaration for getFile (defined in main.c)
// struct limine_file* getFile(const char* name);

// Define EV_CURRENT if not in elf.h (usually 1)
#ifndef EV_CURRENT
#define EV_CURRENT 1
#endif

// Define user memory layout constants
#define USER_STACK_PAGES 8 // Number of pages for the stack (8 * 4KiB = 32KiB)
#define USER_STACK_TOP_VADDR  (0x80000000 - PAGE_SIZE) // Top page starts here (e.g., 0x7FFFF000)
#define USER_STACK_BOTTOM_VADDR (USER_STACK_TOP_VADDR - (USER_STACK_PAGES * PAGE_SIZE))

// Define error codes for exec_elf function
#define EXEC_SUCCESS 0
#define EXEC_FILE_NOT_FOUND 1
#define EXEC_INVALID_ELF 2
#define EXEC_MEMORY_ERROR 3
#define EXEC_MAPPING_ERROR 4
#define EXEC_JUMP_FAILED 5

void exec_elf(const char *filename) {
    serial_write("IN EXEC_ELF\n", 12);
    serial_write("Executing ELF file: ", 20);
    serial_write(filename, strlen(filename));
    serial_write("\n", 1);

    // --- Get file data using filesystem --- 
    // struct limine_file* elf_file_struct = getFile(filename); // OLD: Using Limine's getFile
    struct fs_file* elf_file_struct = fs_open(filename); // NEW: Using our filesystem open
    if (elf_file_struct == NULL) {
        serial_write("Error: File not found via fs_open: ", 35);
        serial_write(filename, strlen(filename));
        serial_write("\n", 1);
        return; // Or handle error appropriately
    }
    // Remove the check for null address, as fs_file doesn't guarantee data allocation on open
    // if (elf_file_struct->address == NULL) { ... }

    // Use file data and size from fs_file struct
    void* elf_data = elf_file_struct->data; 
    size_t elf_size = elf_file_struct->size; 

    serial_write("File loaded via fs_open. Address: 0x", 37);
    serial_print_hex((uint64_t)elf_data);
    serial_write(", Size: ", 8);
    serial_print_hex(elf_size);
    serial_write("\n", 1);

    // --- Validate ELF Header --- 
    if (elf_size < sizeof(elf64_header_t)) { // Use local type
        serial_write("Error: File too small to be ELF header.\n", 41);
        return;
    }
    elf64_header_t *header = (elf64_header_t *)elf_data; // Use local type
    // Check magic, class, data, type, machine, version using local header fields
    if (header->common.e_magic != ELF_MAGIC ||          // Check magic number
        header->common.e_class != ELFCLASS64 ||      // Must be 64-bit
        header->common.e_data != ELFDATA2LSB ||      // Must be little-endian
        header->common.e_type != ET_EXEC ||          // Must be executable
        header->common.e_machine != EM_X86_64 ||     // Must be x86_64
        header->common.e_version != EV_CURRENT) {    // Must be current version
        serial_write("Error: Invalid ELF header fields.\n", 34);
        // Optional: print specific mismatch
        return;
    }

    uint64_t entry_point_vaddr = header->e_entry; // Virtual address from ELF header

    // --- VMM Setup ---
    serial_write("Creating address space...\n", 27);
    pml4_t* user_pml4_phys = vmm_create_address_space();
    if (!user_pml4_phys) {
        serial_write("Error: Failed to create address space for process.\n", 51);
        return; // Cannot proceed
    }

    // --- Load Program Headers (Segments) --- 
    if (elf_size < header->e_phoff + (uint64_t)header->e_phnum * sizeof(elf64_program_header_t)) { // Use local type
        serial_write("Error: File too small for program headers.\n", 43);
        return;
    }
    elf64_program_header_t *phdrs = (elf64_program_header_t *)((uint8_t *)elf_data + header->e_phoff); // Use local type
    serial_write("Loading program segments...\n", 28);
    for (int i = 0; i < header->e_phnum; i++) {
        elf64_program_header_t *ph = &phdrs[i]; // Use local type

        if (ph->p_type != PT_LOAD) { // Check segment type
            continue; // Skip non-loadable segments
        }

        uint64_t segment_virt_addr = ph->p_vaddr;
        uint64_t segment_mem_size = ph->p_memsz;
        uint64_t segment_file_size = ph->p_filesz;
        uint32_t segment_flags = ph->p_flags; // PF_R, PF_W, PF_X

        serial_write("  Segment: VAddr=0x", 19); serial_print_hex(segment_virt_addr);
        serial_write(", MemSize=0x", 13); serial_print_hex(segment_mem_size);
        serial_write(", FileSize=0x", 14); serial_print_hex(segment_file_size);
        serial_write(", Flags=", 8);
        if(segment_flags & PF_R) serial_write("R", 1);
        if(segment_flags & PF_W) serial_write("W", 1);
        if(segment_flags & PF_X) serial_write("X", 1);
        serial_write("\n", 1);

        if (segment_mem_size == 0) continue; // Skip empty segments

        // Align virtual address down to page boundary for mapping loop
        uint64_t first_page_vaddr = segment_virt_addr & PAGE_MASK;
        uint64_t last_page_vaddr = (segment_virt_addr + segment_mem_size + PAGE_SIZE - 1) & PAGE_MASK;
        uint64_t num_pages = (last_page_vaddr - first_page_vaddr) / PAGE_SIZE;
        if (num_pages == 0 && segment_mem_size > 0) num_pages = 1; // Handle small segments within one page

        serial_write("VMM: Mapping segment...\n", 26); // DEBUG
        for (uint64_t offset = 0; offset < ph->p_memsz; offset += PAGE_SIZE) {
            uint64_t page_vaddr = (ph->p_vaddr + offset) & PAGE_MASK;
            
            // Check if this page is already mapped (can happen with overlapping segments? unlikely)
            // Note: vmm_get_physical_address not implemented yet, skip check for now.
            // if (vmm_get_physical_address(user_pml4_phys, current_virt_addr) != 0) {
            //     continue;
            // }
            
            void* phys_frame = pmm_alloc_frame();
            if (!phys_frame) {
                 serial_write("Error: Out of physical memory loading segment.\n", 47);
                 // TODO: Need process cleanup (free allocated frames, page tables)
                 return;
            }

            uint64_t current_phys_addr = (uint64_t)phys_frame;

            // Determine page flags
            uint64_t page_flags = PTE_PRESENT | PTE_USER; // Base flags
            if (segment_flags & PF_W) page_flags |= PTE_WRITABLE;
            if (!(segment_flags & PF_X)) page_flags |= PTE_NX; // No-Execute if not executable

            // Map the page
            if (!vmm_map_page(user_pml4_phys, page_vaddr, current_phys_addr, page_flags)) {
                serial_write("Error: Failed to map page for segment.\n", 40);
                pmm_free_frame(phys_frame); // Free the frame we just allocated
                // TODO: Need process cleanup
                return;
            }
            
            // Copy data from ELF file to the newly allocated physical frame (via virtual addr)
            void* virt_frame_dest = phys_to_virt(current_phys_addr);
            memset(virt_frame_dest, 0, PAGE_SIZE); // Zero the page first (for .bss sections)
            
            // Calculate copy ranges carefully
            uint64_t file_start_offset = ph->p_offset;
            uint64_t segment_start_in_file = file_start_offset;
            uint64_t segment_end_in_file = file_start_offset + segment_file_size;
            
            uint64_t page_start_vaddr = page_vaddr;
            uint64_t page_end_vaddr = page_vaddr + PAGE_SIZE;
            
            uint64_t segment_start_vaddr = segment_virt_addr;
            uint64_t segment_end_vaddr = segment_virt_addr + segment_mem_size;
            
            // Determine intersection of file data range and current page range
            uint64_t copy_start_vaddr = (page_start_vaddr > segment_start_vaddr) ? page_start_vaddr : segment_start_vaddr;
            uint64_t copy_end_vaddr = (page_end_vaddr < segment_end_vaddr) ? page_end_vaddr : segment_end_vaddr;
            
            if (copy_start_vaddr < copy_end_vaddr) { // If there is an overlap
                uint64_t copy_offset_in_page = copy_start_vaddr - page_start_vaddr;
                uint64_t copy_offset_in_segment = copy_start_vaddr - segment_start_vaddr;
                uint64_t file_offset = segment_start_in_file + copy_offset_in_segment;
                size_t bytes_to_copy = copy_end_vaddr - copy_start_vaddr;

                // Ensure we don't read past the file size allocated to the segment
                if (file_offset + bytes_to_copy > segment_end_in_file) {
                    if (file_offset >= segment_end_in_file) {
                        bytes_to_copy = 0; // No file data left in this range
                    } else {
                        bytes_to_copy = segment_end_in_file - file_offset;
                    }
                }
                
                if (bytes_to_copy > 0) {
                    memcpy((uint8_t*)virt_frame_dest + copy_offset_in_page,
                           (uint8_t *)elf_data + file_offset,
                           bytes_to_copy);
                }
            }
            
            // serial_write("    Mapped V=0x", 16); serial_print_hex(current_virt_addr);
            // serial_write(" -> P=0x", 9); serial_print_hex(current_phys_addr);
            // serial_write(" Flags=0x", 10); serial_print_hex(page_flags);
            // serial_write("\n", 1);
        }
    }
    serial_write("ELF Segments loaded and mapped.\n", 31);

    // --- Allocate and Map User Stack ---
    serial_write("[EXEC] Allocating user stack...\n", 30);
    uint64_t user_rsp = USER_STACK_TOP_VADDR + PAGE_SIZE - 8;
    for (uint64_t vaddr = USER_STACK_BOTTOM_VADDR; vaddr <= USER_STACK_TOP_VADDR; vaddr += PAGE_SIZE) {
        void* phys_frame = pmm_alloc_frame();
        if (!phys_frame) {
            serial_write("Error: Out of physical memory allocating stack.\n", 48);
            // TODO: Cleanup
            return;
        }
        uint64_t stack_flags = PTE_PRESENT | PTE_USER | PTE_WRITABLE; // REMOVED PTE_NX
        int map_result = vmm_map_page(user_pml4_phys, vaddr, (uint64_t)phys_frame, stack_flags);
        serial_write("[STACK MAP] vaddr=0x", 20); serial_print_hex(vaddr);
        serial_write(" phys=0x", 8); serial_print_hex((uint64_t)phys_frame);
        serial_write(" result=", 9); serial_print_hex(map_result);
        serial_write("\n", 1);
        if (!map_result) {
             serial_write("Error: Failed to map page for stack.\n", 37);
             pmm_free_frame(phys_frame);
             // TODO: Cleanup
             return;
         }
        // serial_write("    Mapped Stack V=0x", 22); serial_print_hex(vaddr);
        // serial_write(" -> P=0x", 9); serial_print_hex((uint64_t)phys_frame);
        // serial_write("\n", 1);
    }

    // Save current kernel address space before switching
    pml4_t* kernel_pml4_phys = vmm_get_current_address_space();

    // Switch to the new process's address space
    vmm_switch_address_space(user_pml4_phys);
    serial_write("[EXEC] CR3 switched.\n", 19);

    serial_write("[EXEC] About to JMP to user mode...\n", 36);
    serial_write("[EXEC] RIP=0x", 12); serial_print_hex(entry_point_vaddr); 
    serial_write(" RSP=0x", 8); serial_print_hex(user_rsp);
    serial_write("\n", 1);

    // Jump to user mode - note that this function is declared as void
    jmp_usermode(entry_point_vaddr, user_rsp);
    
    // If we get here, the user program has exited via syscall or jmp_usermode returned unexpectedly
    serial_write("[EXEC] Process returned from usermode, cleaning up...\n", 55);
    
    // 1. Restore kernel address space
    vmm_switch_address_space(kernel_pml4_phys);
    
    // 2. Free process memory - release all pages allocated for this process
    // For now, we'll just free the user stack pages since we know their range
    serial_write("[EXEC] Freeing user stack memory...\n", 36);
    for (uint64_t vaddr = USER_STACK_BOTTOM_VADDR; vaddr < USER_STACK_TOP_VADDR; vaddr += PAGE_SIZE) {
        // Get the physical address for this virtual address
        void* phys_addr = (void*)vmm_get_physical_address(user_pml4_phys, vaddr);
        if (phys_addr) {
            // Free the physical frame
            pmm_free_frame(phys_addr);
            
            // Unmap from page tables
            vmm_unmap_page(user_pml4_phys, vaddr);
        }
    }
    
    // 3. TODO: Free the user page tables themselves (for now we'll leak these)
    // This would involve walking the page tables and freeing each level
    
    serial_write("[EXEC] Process cleanup complete\n", 32);
    return;
}
