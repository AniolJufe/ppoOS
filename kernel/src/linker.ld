OUTPUT_FORMAT(elf64-x86-64)
ENTRY(kmain)

SECTIONS
{
    . = 0xffffffff80000000;
    .text : {
        *(.text*)
    }
    .rodata : {
        *(.rodata*)
    }
    .data : {
        *(.data*)
    }
    .bss : {
        *(.bss*)
        *(COMMON)
    }
    . = ALIGN(0x1000);
    kernel_stack = .;
    . += 0x8000; /* 32KB kernel stack */
    kernel_stack_top = .;
}
PROVIDE(kernel_stack_top = kernel_stack_top);
