/* Ф0.2 bench: everything in the 4 MB board SRAM at 0, MMIO at 0x80000000 (VexRiscv IO range = bit 31). */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY { SRAM (rwx) : ORIGIN = 0x00000000, LENGTH = 4M }

STACK_SIZE = 128K;

SECTIONS
{
    .text : {
        KEEP(*(.text.start))
        *(.text .text.*)
        *(.gnu.linkonce.t.*)
    } > SRAM
    .rodata : ALIGN(8) {
        *(.rodata .rodata.*)
        *(.srodata .srodata.*)
    } > SRAM
    .eh_frame_hdr : { *(.eh_frame_hdr) } > SRAM
    .eh_frame : { __eh_frame_start = .; KEEP(*(.eh_frame)) LONG(0) } > SRAM
    .gcc_except_table : { *(.gcc_except_table .gcc_except_table.*) } > SRAM
    .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array)) PROVIDE_HIDDEN(__preinit_array_end = .); } > SRAM
    .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array)) PROVIDE_HIDDEN(__init_array_end = .); } > SRAM
    .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array)) PROVIDE_HIDDEN(__fini_array_end = .); } > SRAM
    .data : ALIGN(8) {
        *(.data .data.*)
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.*)
    } > SRAM
    .bss (NOLOAD) : ALIGN(8) {
        __bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end = .;
    } > SRAM
    __heap_start = .;
    /* last 4 KB: resume stub + registers written by sim/iss --dump-at (fast-forward), never used here */
    __resume = ORIGIN(SRAM) + LENGTH(SRAM) - 4K;
    __stack_top = __resume;
    __heap_end = __stack_top - STACK_SIZE;
}
