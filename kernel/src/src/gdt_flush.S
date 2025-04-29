# gdt_flush.S - Assembly implementation for GDT and TSS loading
    .text
    .global gdt_flush
    .type gdt_flush, @function
gdt_flush:
    lgdt (%rdi)
    mov $0x10, %ax      # Kernel data selector
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    pushq $0x08         # Kernel code selector
    lea 1f(%rip), %rax
    pushq %rax
    lretq               # Far return to reload CS
1:
    ret

    .global tss_flush
    .type tss_flush, @function
tss_flush:
    mov $0x2B, %ax      # TSS selector (GDT entry 5 << 3 | 3)
    ltr %ax
    ret
