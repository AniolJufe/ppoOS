#include "vmm.h"
#include "serial.h"
#include "lib/string.h"
#include <stdbool.h>
#include <stddef.h>
#include "limine.h" // Include Limine header for request structures

// Extern declarations for Limine requests defined in main.c
extern volatile struct limine_hhdm_request hhdm_request;
extern volatile struct limine_memmap_request memmap_request;
extern volatile struct limine_kernel_address_request kernel_address_request;

// --- Physical Memory Manager (PMM) --- 

// Reference to the memory map request (defined in main.c)
// extern volatile struct limine_memmap_request memmap_request;
// Reference to the kernel address request (defined in main.c)
// extern volatile struct limine_kernel_address_request kernel_address_request;
// Reference to the HHDM request (defined in main.c)
// extern volatile struct limine_hhdm_request hhdm_request;

// Simple bitmap-based physical memory allocator
// TODO: Find a better place for the bitmap itself!
// For now, let's assume a max physical address and put the bitmap somewhere safe.
// We need to calculate the required bitmap size based on the highest available address.
#define PMM_MAX_PHYS_ADDR (4ULL * 1024 * 1024 * 1024) // Assume max 4GiB for now
#define PMM_BITMAP_SIZE ((PMM_MAX_PHYS_ADDR / PAGE_SIZE) / 8) // Bytes needed

// WARNING: Placing the bitmap at a fixed address is DANGEROUS.
// This needs to be dynamically placed in a safe area identified by the memmap.
// This is just a placeholder for initial implementation.
static uint8_t pmm_bitmap[PMM_BITMAP_SIZE] __attribute__((aligned(PAGE_SIZE)));

static size_t pmm_last_alloc_index = 0;
static uint64_t pmm_highest_address = 0;

// Helper functions for bitmap manipulation
static inline void pmm_bitmap_set(size_t bit) {
    pmm_bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void pmm_bitmap_unset(size_t bit) {
    pmm_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline bool pmm_bitmap_test(size_t bit) {
    return (pmm_bitmap[bit / 8] & (1 << (bit % 8))) != 0;
}

// --- Helper Functions ---

static uint64_t hhdm_offset = 0; // Cached HHDM offset

// Converts a physical address to a virtual address using the HHDM offset
void* phys_to_virt(uint64_t phys_addr) {
    if (hhdm_offset == 0) {
        if (hhdm_request.response == NULL) {
            serial_write("VMM Error: HHDM request has no response! Cannot convert phys->virt.\n", 69);
            // TODO: Handle this critical error
            for(;;); 
        }
        hhdm_offset = hhdm_request.response->offset;
        serial_write("VMM: Cached HHDM offset: 0x", 26);
        serial_print_hex(hhdm_offset);
        serial_write("\n", 1);
    }
    return (void*)(phys_addr + hhdm_offset);
}

void pmm_init(void) {
    if (memmap_request.response == NULL) {
        serial_write("PMM Error: No memory map response from Limine!\n", 47);
        // TODO: Halt or handle error appropriately
        for(;;); 
    }

    struct limine_memmap_entry **entries = memmap_request.response->entries;
    uint64_t entry_count = memmap_request.response->entry_count;

    // 1. Initialize bitmap: mark all memory as used initially.
    memset(pmm_bitmap, 0xFF, PMM_BITMAP_SIZE);

    serial_write("PMM: Initializing...\n", 22);

    // 2. Iterate through memory map and mark usable frames as free.
    for (uint64_t i = 0; i < entry_count; i++) {
        struct limine_memmap_entry *entry = entries[i];

        // Debug print entry info
        // serial_write("  Memmap Entry: Base=0x", 23);
        // serial_print_hex(entry->base);
        // serial_write(", Length=0x", 12);
        // serial_print_hex(entry->length);
        // serial_write(", Type=", 8);
        // serial_print_hex(entry->type);
        // serial_write("\n", 1);

        if (entry->type == LIMINE_MEMMAP_USABLE) {
            // Mark all full frames within this usable range as free
            uint64_t start_frame = (entry->base + PAGE_SIZE - 1) / PAGE_SIZE; // Align up
            uint64_t end_frame = (entry->base + entry->length) / PAGE_SIZE; // Align down

            for (uint64_t frame = start_frame; frame < end_frame; frame++) {
                size_t bit_index = frame;
                if ((frame * PAGE_SIZE) >= PMM_MAX_PHYS_ADDR) continue; // Skip if beyond our bitmap limit
                
                pmm_bitmap_unset(bit_index);
                
                // Track highest available address for bitmap sizing later
                if ((frame + 1) * PAGE_SIZE > pmm_highest_address) {
                    pmm_highest_address = (frame + 1) * PAGE_SIZE;
                }
            }
            // serial_write("    Marked frames ", 18);
            // serial_print_hex(start_frame);
            // serial_write(" to ", 4);
            // serial_print_hex(end_frame - 1);
            // serial_write(" as usable.\n", 13);
        }
    }
    
    // 3. Mark kernel memory as used.
    if (kernel_address_request.response == NULL) {
        serial_write("PMM Error: No kernel address response from Limine!\n", 51);
        // TODO: Halt or handle error appropriately
        for(;;);
    }
    
    uint64_t kernel_phys_base = kernel_address_request.response->physical_base;
    uint64_t kernel_virt_base = kernel_address_request.response->virtual_base;
    
    // We need the *size* of the kernel. Limine doesn't directly provide this,
    // we often get it from linker symbols (_kernel_start, _kernel_end).
    // For now, let's make a reasonable guess or use a placeholder size.
    // Assume the kernel occupies memory from its physical base up to the start
    // of the bitmap we statically placed. THIS IS A HACK.
    // A better way involves linker script symbols.
    extern uint8_t _kernel_end[]; // Symbol from linker script (needs to be defined there)
    uint64_t kernel_size = (uint64_t)_kernel_end - kernel_virt_base; // Calculate size based on virtual addresses
    // Ensure a minimum size if the symbol isn't right or calculation is off
    if (kernel_size == 0 || kernel_size > (512 * 1024 * 1024)) { // Sanity check (e.g., > 512MiB is suspicious)
        serial_write("PMM Warning: Kernel size calculation seems off. Using 16MiB placeholder.\n", 70);
        kernel_size = 16 * 1024 * 1024; // Use a 16MiB placeholder size
    }

    uint64_t kernel_start_frame = kernel_phys_base / PAGE_SIZE;
    uint64_t kernel_end_frame = (kernel_phys_base + kernel_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    serial_write("PMM: Marking kernel frames [", 27);
    serial_print_hex(kernel_start_frame);
    serial_write(" - ", 3);
    serial_print_hex(kernel_end_frame - 1);
    serial_write("] (phys 0x", 11);
    serial_print_hex(kernel_phys_base);
    serial_write(") as used...\n", 14);
    
    for (uint64_t frame = kernel_start_frame; frame < kernel_end_frame; frame++) {
        size_t bit_index = frame;
        if ((frame * PAGE_SIZE) >= PMM_MAX_PHYS_ADDR) continue; // Skip if beyond our bitmap limit
        if (!pmm_bitmap_test(bit_index)) {
             //serial_write("PMM Info: Kernel frame ", 24);
             //serial_print_hex(frame);
             //serial_write(" was free, marking used.\n", 26);
        }
        pmm_bitmap_set(bit_index);
    }
    
    // TODO: Mark bitmap memory itself as used! Requires knowing where bitmap is.
    uint64_t bitmap_phys_addr = (uint64_t)pmm_bitmap; // HACK: Assumes identity mapping / HHDM
    uint64_t bitmap_start_frame = bitmap_phys_addr / PAGE_SIZE;
    uint64_t bitmap_end_frame = (bitmap_phys_addr + PMM_BITMAP_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    serial_write("PMM: Marking bitmap frames [", 27);
    serial_print_hex(bitmap_start_frame);
    serial_write(" - ", 3);
    serial_print_hex(bitmap_end_frame - 1);
    serial_write("] (phys 0x", 11);
    serial_print_hex(bitmap_phys_addr);
    serial_write(") as used...\n", 14);
    for (uint64_t frame = bitmap_start_frame; frame < bitmap_end_frame; frame++) {
        size_t bit_index = frame;
        if ((frame * PAGE_SIZE) >= PMM_MAX_PHYS_ADDR) continue;
        if (!pmm_bitmap_test(bit_index)) {
            // serial_write("PMM Info: Bitmap frame ", 23);
            // serial_print_hex(frame);
            // serial_write(" was free, marking used.\n", 26);
        }
        pmm_bitmap_set(bit_index);
    }

    serial_write("PMM: Initialization complete. Highest address: 0x", 49);
    serial_print_hex(pmm_highest_address);
    serial_write("\n", 1);
    
    // Reset last alloc index for the first allocation
    pmm_last_alloc_index = 0;
}

// Allocates one physical 4KiB frame
// Returns physical address of the frame, or NULL if out of memory
void* pmm_alloc_frame(void) {
    size_t max_bits = (pmm_highest_address / PAGE_SIZE);
    
    // Simple linear scan for a free bit (can be optimized)
    for (size_t i = 0; i < max_bits; ++i) {
        size_t current_index = (pmm_last_alloc_index + i) % max_bits;
        
        // Skip low memory (below 1MB often problematic)
        if (current_index * PAGE_SIZE < 0x100000) continue; 
        
        if (!pmm_bitmap_test(current_index)) {
            pmm_bitmap_set(current_index);
            pmm_last_alloc_index = current_index + 1; // Start next search from here
            uint64_t phys_addr = (uint64_t)current_index * PAGE_SIZE;
            // serial_write("PMM Alloc: 0x", 14);
            // serial_print_hex(phys_addr);
            // serial_write("\n", 1);
            return (void*)phys_addr;
        }
    }
    
    serial_write("PMM Error: Out of physical memory!\n", 35);
    return NULL; // Out of memory
}

// Frees a physical frame
void pmm_free_frame(void* frame_addr) {
    uint64_t phys_addr = (uint64_t)frame_addr;
    if (phys_addr % PAGE_SIZE != 0) {
        serial_write("PMM Error: Attempted to free non-page-aligned address 0x", 57);
        serial_print_hex(phys_addr);
        serial_write("\n", 1);
        return;
    }
    
    size_t bit_index = phys_addr / PAGE_SIZE;
    if (bit_index * PAGE_SIZE >= pmm_highest_address) {
        serial_write("PMM Error: Attempted to free address outside managed range 0x", 61);
        serial_print_hex(phys_addr);
        serial_write("\n", 1);
        return;
    }
    
    if (!pmm_bitmap_test(bit_index)) {
        serial_write("PMM Warning: Attempted to double-free frame 0x", 46);
        serial_print_hex(phys_addr);
        serial_write("\n", 1);
        // Proceed with unsetting anyway, maybe?
    }
    
    pmm_bitmap_unset(bit_index);
    // serial_write("PMM Free: 0x", 13);
    // serial_print_hex(phys_addr);
    // serial_write("\n", 1);
}

// --- Virtual Memory Management (VMM) ---

// Global variable holding the physical address of the kernel's top-level PML4 table
pml4_t* g_kernel_pml4 = NULL;

// TODO: Implement VMM functions using PMM

void vmm_init(void) {
    // Read the initial kernel PML4 physical address from CR3
    uint64_t cr3_val;
    asm volatile ("mov %%cr3, %0" : "=r"(cr3_val));
    g_kernel_pml4 = (pml4_t*)(cr3_val & PAGE_MASK);

    serial_write("VMM: Stored kernel PML4 address: 0x", 34);
    serial_print_hex((uint64_t)g_kernel_pml4);
    serial_write("\n", 1);

    // Perform other VMM initialization if needed
}

pml4_t* vmm_create_address_space(void) {
    // Get the physical address of the current kernel PML4
    // uint64_t kernel_pml4_phys_addr;
    // asm volatile ("mov %%cr3, %0" : "=r"(kernel_pml4_phys_addr));
    // kernel_pml4_phys_addr &= PAGE_MASK; // Mask out flags

    // Use the stored global kernel PML4 address
    if (!g_kernel_pml4) {
        serial_write("VMM Error: Kernel PML4 global is NULL! Did vmm_init run?\n", 59);
        // Consider halting or returning NULL
        for(;;); // Halt for now
    }
    uint64_t kernel_pml4_phys_addr = (uint64_t)g_kernel_pml4;

    // Allocate a frame for the new PML4 table
    pml4_t* user_pml4_phys = (pml4_t*)pmm_alloc_frame();
    if (!user_pml4_phys) {
        serial_write("VMM Error: Failed to allocate PML4 frame!\n", 43);
        return NULL;
    }

    // Get virtual addresses for both PML4s using HHDM
    pml4_t* kernel_pml4_virt = (pml4_t*)phys_to_virt(kernel_pml4_phys_addr);
    pml4_t* user_pml4_virt = (pml4_t*)phys_to_virt((uint64_t)user_pml4_phys);

    // Zero out the new user PML4 table
    memset(user_pml4_virt, 0, PAGE_SIZE);

    // Copy kernel mappings (higher half, e.g., entries 256-511)
    // Adjust the range if your kernel isn't purely in the higher half
    serial_write("VMM: Copying kernel mappings...\n", 31);
    for (int i = 256; i < 512; i++) {
        if (kernel_pml4_virt->entries[i] & PTE_PRESENT) {
            user_pml4_virt->entries[i] = kernel_pml4_virt->entries[i];
        }
    }

    serial_write("VMM: Created new address space (PML4) at phys 0x", 49);
    serial_print_hex((uint64_t)user_pml4_phys);
    serial_write("\n", 1);
    return user_pml4_phys; // Return physical address
}

// Placeholder - needs actual implementation
bool vmm_map_page(pml4_t* pml4, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags) {
    // 1. Calculate indices for each level
    uint64_t pml4_index = (virt_addr >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt_addr >> 30) & 0x1FF;
    uint64_t pd_index   = (virt_addr >> 21) & 0x1FF;
    uint64_t pt_index   = (virt_addr >> 12) & 0x1FF;

    // Get virtual address of the PML4 table itself
    pml4_t* pml4_virt = phys_to_virt((uint64_t)pml4);

    // 2. Walk PML4
    pml4e_t* pml4e = &pml4_virt->entries[pml4_index];
    pdpt_t* pdpt_virt;
    if (!(*pml4e & PTE_PRESENT)) {
        // PDPT not present, allocate one
        void* pdpt_phys = pmm_alloc_frame();
        if (!pdpt_phys) return false; // Out of memory
        pdpt_virt = phys_to_virt((uint64_t)pdpt_phys);
        memset(pdpt_virt, 0, PAGE_SIZE);
        *pml4e = (uint64_t)pdpt_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER; // Assume user accessible for now
    } else {
        pdpt_virt = phys_to_virt(*pml4e & PAGE_MASK);
    }

    // 3. Walk PDPT
    pdpte_t* pdpte = &pdpt_virt->entries[pdpt_index];
    pd_t* pd_virt;
    if (!(*pdpte & PTE_PRESENT)) {
        // PD not present, allocate one
        void* pd_phys = pmm_alloc_frame();
        if (!pd_phys) return false; // Out of memory
        pd_virt = phys_to_virt((uint64_t)pd_phys);
        memset(pd_virt, 0, PAGE_SIZE);
        *pdpte = (uint64_t)pd_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else {
        // TODO: Check if this is a 1GiB huge page? Not handled here.
        pd_virt = phys_to_virt(*pdpte & PAGE_MASK);
    }

    // 4. Walk PD
    pde_t* pde = &pd_virt->entries[pd_index];
    pt_t* pt_virt;
    if (!(*pde & PTE_PRESENT)) {
        // PT not present, allocate one
        void* pt_phys = pmm_alloc_frame();
        if (!pt_phys) return false; // Out of memory
        pt_virt = phys_to_virt((uint64_t)pt_phys);
        memset(pt_virt, 0, PAGE_SIZE);
        *pde = (uint64_t)pt_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    } else {
        // TODO: Check if this is a 2MiB huge page? Not handled here.
        pt_virt = phys_to_virt(*pde & PAGE_MASK);
    }

    // 5. Set the PTE entry in the Page Table
    pte_t* pte = &pt_virt->entries[pt_index];
    if (*pte & PTE_PRESENT) {
        // TODO: Should we allow re-mapping? Maybe log a warning?
        serial_write("VMM Warning: Re-mapping existing page at virt 0x", 47);
        serial_print_hex(virt_addr);
        serial_write("\n", 1);
    }
    *pte = phys_addr | flags; // Apply the provided flags (PTE_PRESENT must be included in flags)

    // serial_write("VMM Map: virt 0x", 16);
    // serial_print_hex(virt_addr);
    // serial_write(" -> phys 0x", 11);
    // serial_print_hex(phys_addr);
    // serial_write(" flags 0x", 10);
    // serial_print_hex(flags);
    // serial_write("\n", 1);

    return true; 
}

void vmm_unmap_page(pml4_t* pml4, uint64_t virt_addr) {
    uint64_t pml4_index = (virt_addr >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt_addr >> 30) & 0x1FF;
    uint64_t pd_index   = (virt_addr >> 21) & 0x1FF;
    uint64_t pt_index   = (virt_addr >> 12) & 0x1FF;

    pml4_t* pml4_virt = phys_to_virt((uint64_t)pml4);
    pml4e_t* pml4e = &pml4_virt->entries[pml4_index];
    if (!(*pml4e & PTE_PRESENT)) {
        return;
    }

    pdpt_t* pdpt_virt = phys_to_virt(*pml4e & PAGE_MASK);
    pdpte_t* pdpte = &pdpt_virt->entries[pdpt_index];
    if (!(*pdpte & PTE_PRESENT)) {
        return;
    }

    pd_t* pd_virt = phys_to_virt(*pdpte & PAGE_MASK);
    pde_t* pde = &pd_virt->entries[pd_index];
    if (!(*pde & PTE_PRESENT)) {
        return;
    }

    pt_t* pt_virt = phys_to_virt(*pde & PAGE_MASK);
    pte_t* pte = &pt_virt->entries[pt_index];
    if (!(*pte & PTE_PRESENT)) {
        return;
    }

    *pte = 0;
    asm volatile ("invlpg (%0)" :: "r" (virt_addr) : "memory");
}

uint64_t vmm_get_physical_address(pml4_t* pml4, uint64_t virt_addr) {
    uint64_t pml4_index = (virt_addr >> 39) & 0x1FF;
    uint64_t pdpt_index = (virt_addr >> 30) & 0x1FF;
    uint64_t pd_index   = (virt_addr >> 21) & 0x1FF;
    uint64_t pt_index   = (virt_addr >> 12) & 0x1FF;

    pml4_t* pml4_virt = phys_to_virt((uint64_t)pml4);
    pml4e_t pml4e = pml4_virt->entries[pml4_index];
    if (!(pml4e & PTE_PRESENT)) {
        return 0;
    }
    pdpt_t* pdpt_virt = phys_to_virt(pml4e & PAGE_MASK);
    pdpte_t pdpte = pdpt_virt->entries[pdpt_index];
    if (!(pdpte & PTE_PRESENT)) {
        return 0;
    }
    pd_t* pd_virt = phys_to_virt(pdpte & PAGE_MASK);
    pde_t pde = pd_virt->entries[pd_index];
    if (!(pde & PTE_PRESENT)) {
        return 0;
    }
    pt_t* pt_virt = phys_to_virt(pde & PAGE_MASK);
    pte_t pte = pt_virt->entries[pt_index];
    if (!(pte & PTE_PRESENT)) {
        return 0;
    }

    uint64_t phys_page = pte & PAGE_MASK;
    return phys_page | (virt_addr & ~PAGE_MASK);
}

void vmm_switch_address_space(pml4_t* pml4) {
    uint64_t pml4_phys = (uint64_t)pml4; // Assuming pml4 holds the physical address
    serial_write("VMM: Switching CR3 to 0x", 25);
    serial_print_hex(pml4_phys);
    serial_write("\n", 1);
    asm volatile ("mov %0, %%cr3" : : "r"(pml4_phys) : "memory");
}

pml4_t* vmm_get_current_address_space(void) {
    uint64_t cr3_val;
    asm volatile ("mov %%cr3, %0" : "=r"(cr3_val));
    return (pml4_t*)(cr3_val & PAGE_MASK); // CR3 contains flags too, mask them off
}
