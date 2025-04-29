/* Simple linker script for our userspace programs */
ENTRY(_start)

PHDRS
{
   text PT_LOAD ;  /* Loadable segment for text/rodata */
   data PT_LOAD ;  /* Loadable segment for data/bss */
   /* PT_PHDR is often implicitly handled or not needed if AT_PHDR is set correctly */
}

SECTIONS
{
    /* Start VMA at 0x1000 */
    . = 0x1000;

    /* Code and read-only data */
    .text : ALIGN(CONSTANT (MAXPAGESIZE)) {
        _text_start = .;
        *(.text .text.*)
        *(.rodata .rodata.*)
        _text_end = .;
    } :text /* Assign to 'text' segment */

    /* Read-write data (initialized and uninitialized) */
    /* Align data to a new page for clarity/protection (optional) */
    . = ALIGN(CONSTANT (MAXPAGESIZE));
    .data : {
        _data_start = .;
        *(.data .data.*)
        _data_end = .;
    } :data /* Assign to 'data' segment */

    .bss : {
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    } :data /* Assign BSS also to 'data' segment */

    /* Discard unnecessary sections */
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}
