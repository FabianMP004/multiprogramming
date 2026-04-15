/* =============================================================================
 * p1.ld — Linker script for User Process 1 (P1)
 *
 * Memory layout:
 *   0x82100000  +---------------------------------------+
 *               | .text     (P1 code + entry point)    |
 *               | .rodata                               |
 *               | .data                                 |
 *               | .bss                                  |
 *   0x82110000  +---------------------------------------+
 *               | P1 stack  (8 KB, grows down)          |
 *   0x82112000  +---------------------------------------+  <-- P1_stack_top
 *
 * The OS PCB for P1 is initialized with:
 *   PC  = 0x82100000  (entry point = first instruction of .text)
 *   SP  = 0x82112000  (top of P1 stack — sets the initial stack frame)
 * ============================================================================= */

ENTRY(main)

MEMORY
{
    P1_CODE (rwx) : ORIGIN = 0x82100000, LENGTH = 64K
    P1_STACK (rw) : ORIGIN = 0x82110000, LENGTH = 8K
}

SECTIONS
{
    .text 0x82100000 :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } > P1_CODE

    .data :
    {
        *(.data)
        *(.data.*)
    } > P1_CODE

    .bss :
    {
        __p1_bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __p1_bss_end = .;
    } > P1_CODE

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __p1_stack_bottom = .;
        . += 0x2000;           /* 8 KB */
        __p1_stack_top = .;
    } > P1_STACK

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
