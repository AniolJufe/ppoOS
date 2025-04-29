#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// --- Page Table Entry Flags ---
#define PTE_PRESENT         (1ULL << 0)  // Present
#define PTE_WRITABLE        (1ULL << 1)  // Read/Write
#define PTE_USER            (1ULL << 2)  // User/Supervisor
#define PTE_WRITE_THROUGH   (1ULL << 3)  // Write-Through
#define PTE_CACHE_DISABLE   (1ULL << 4)  // Cache Disable
#define PTE_ACCESSED        (1ULL << 5)  // Accessed
#define PTE_DIRTY           (1ULL << 6)  // Dirty
#define PTE_PAT             (1ULL << 7)  // Page Attribute Table
#define PTE_GLOBAL          (1ULL << 8)  // Global
#define PTE_NX              (1ULL << 63) // No Execute (Execute Disable)

#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))

// Structure for a Page Map Level 4 Entry (PML4E) and Page Directory Pointer Table Entry (PDPTE)
// Also used for Page Directory Entry (PDE) pointing to a Page Table (PT)
typedef uint64_t pml4e_t;
typedef uint64_t pdpte_t;
typedef uint64_t pde_t;

// Structure for a Page Table Entry (PTE) pointing to a physical page
typedef uint64_t pte_t;

// Page Table Structures (representing the 4KiB tables themselves)
typedef struct {
    pml4e_t entries[512];
} __attribute__((aligned(PAGE_SIZE))) pml4_t;

typedef struct {
    pdpte_t entries[512];
} __attribute__((aligned(PAGE_SIZE))) pdpt_t;

typedef struct {
    pde_t entries[512];
} __attribute__((aligned(PAGE_SIZE))) pd_t;

typedef struct {
    pte_t entries[512];
} __attribute__((aligned(PAGE_SIZE))) pt_t;

// --- Physical Memory Management (Placeholder) ---
void pmm_init(void); // TODO: Needs memory map info from Limine
void* pmm_alloc_frame(void); // Allocates one physical 4KiB frame
void pmm_free_frame(void* frame);

// --- Virtual Memory Management ---

// Creates a new, empty PML4 table (kernel mappings might be added later)
pml4_t* vmm_create_address_space(void);

// Maps a virtual address to a physical address in the given PML4
// Allocates page tables as needed
// Returns true on success, false on failure (e.g., out of memory)
bool vmm_map_page(pml4_t* pml4, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

// Unmaps a virtual address
void vmm_unmap_page(pml4_t* pml4, uint64_t virt_addr);

// Gets the physical address corresponding to a virtual address in the given PML4
// Returns 0 if not mapped
uint64_t vmm_get_physical_address(pml4_t* pml4, uint64_t virt_addr);

// Loads the given PML4 into CR3
void vmm_switch_address_space(pml4_t* pml4);

// Gets the currently active PML4 from CR3
pml4_t* vmm_get_current_address_space(void);

// Helper to convert physical address to virtual using HHDM
void* phys_to_virt(uint64_t phys_addr);

// Global variable holding the physical address of the kernel's top-level PML4 table
extern pml4_t* g_kernel_pml4;
