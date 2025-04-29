#include "gdt.h"
#include <stddef.h>
#include <stdint.h>
#include "serial.h"

#define GDT_ENTRIES 7

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gp;

// 64-bit TSS descriptor is 16 bytes, so we need to place it after the normal entries.
struct __attribute__((packed)) gdt_tss_desc {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle1;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_middle2;
    uint32_t base_high;
    uint32_t reserved;
};

static struct tss_entry tss __attribute__((aligned(16)));

extern void gdt_flush(uint64_t);
extern void tss_flush();

static void set_gdt_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].access = access;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[num].base_high = (base >> 24) & 0xFF;
}

void gdt_init(void) {
    serial_write("GDT: Initializing...\n", 22);
    // Null segment
    set_gdt_entry(0, 0, 0, 0, 0);
    // Kernel code: base=0, limit=0xFFFFF, access=0x9A, gran=0xA0 (L=1, G=1, D=0)
    set_gdt_entry(1, 0, 0xFFFFF, 0x9A, 0xA0);
    // Kernel data: base=0, limit=0xFFFFF, access=0x92, gran=0xC0 (G=1, D=1)
    set_gdt_entry(2, 0, 0xFFFFF, 0x92, 0xC0);
    // User code: base=0, limit=0xFFFFF, access=0xFA, gran=0xA0 (L=1, G=1, D=0, DPL=3)
    set_gdt_entry(3, 0, 0xFFFFF, 0xFA, 0xA0);
    // User data: base=0, limit=0xFFFFF, access=0xF2, gran=0xC0 (G=1, D=1, DPL=3)
    set_gdt_entry(4, 0, 0xFFFFF, 0xF2, 0xC0);
    set_gdt_entry(5, 0, 0, 0, 0);               // Placeholder for TSS (part 1)
    set_gdt_entry(6, 0, 0, 0, 0);               // Placeholder for TSS (part 2)

    serial_write("User code GDT[3]: access=0x", 28);
    serial_print_hex(gdt[3].access);
    serial_write(" gran=0x", 9);
    serial_print_hex(gdt[3].granularity);
    serial_write("\n", 1);
    serial_write("User data GDT[4]: access=0x", 28);
    serial_print_hex(gdt[4].access);
    serial_write(" gran=0x", 9);
    serial_print_hex(gdt[4].granularity);
    serial_write("\n", 1);

    gp.limit = sizeof(gdt) - 1;
    gp.base = (uint64_t)&gdt;

    // Setup TSS
    for (int i = 0; i < sizeof(struct tss_entry); ++i) ((uint8_t*)&tss)[i] = 0;
    extern uint8_t kernel_stack_top[];
    tss.rsp0 = (uint64_t)kernel_stack_top;
    tss.iomap_base = sizeof(struct tss_entry);

    // Setup TSS descriptor in GDT
    uint64_t tss_base = (uint64_t)&tss;
    uint16_t tss_limit = sizeof(struct tss_entry) - 1;
    uint8_t *desc = (uint8_t *)&gdt[5];
    desc[0] = tss_limit & 0xFF;
    desc[1] = (tss_limit >> 8) & 0xFF;
    desc[2] = tss_base & 0xFF;
    desc[3] = (tss_base >> 8) & 0xFF;
    desc[4] = (tss_base >> 16) & 0xFF;
    desc[5] = 0x89; // Present, type 9 (64-bit TSS)
    desc[6] = 0;
    desc[7] = ((tss_limit >> 16) & 0x0F) | ((tss_base >> 24) & 0xF0);
    desc[8] = (tss_base >> 32) & 0xFF;
    desc[9] = (tss_base >> 40) & 0xFF;
    desc[10] = (tss_base >> 48) & 0xFF;
    desc[11] = (tss_base >> 56) & 0xFF;
    desc[12] = 0;
    desc[13] = 0;
    desc[14] = 0;
    desc[15] = 0;

    gdt_flush((uint64_t)&gp);
    tss_flush();
    serial_write("GDT: Initialized\n", 17);
}
