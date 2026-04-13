/* =============================================================================
 * p2.ld — Linker script for User Process 2 (P2)
 *
 * Memory layout:
 *   0x82200000  +---------------------------------------+
 *               | .text     (P2 code + entry point)    |
 *               | .rodata                               |
 *               | .data                                 |
 *               | .bss                                  |
 *   0x82210000  +---------------------------------------+
 *               | P2 stack  (8 KB, grows down)          |
 *   0x82212000  +---------------------------------------+  <-- P2_stack_top
 *
 * The OS PCB for P2 is initialized with:
 *   PC  = 0x82200000  (entry point)
 *   SP  = 0x82212000  (top of P2 stack — Dev B sets the initial stack frame)
 * ============================================================================= */

ENTRY(main)

MEMORY
{
    P2_CODE (rwx) : ORIGIN = 0x82200000, LENGTH = 64K
    P2_STACK (rw) : ORIGIN = 0x82210000, LENGTH = 8K
}

SECTIONS
{
    .text 0x82200000 :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } > P2_CODE

    .data :
    {
        *(.data)
        *(.data.*)
    } > P2_CODE

    .bss :
    {
        __p2_bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __p2_bss_end = .;
    } > P2_CODE

    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __p2_stack_bottom = .;
        . += 0x2000;           /* 8 KB */
        __p2_stack_top = .;
    } > P2_STACK

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
