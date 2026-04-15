/* =============================================================================
 * os.ld — Linker script for the OS image
 *
 * Memory layout:
 *   0x82000000  +---------------------------------------+
 *               | .vectors  (vector table, 32 bytes)    |
 *               | .text     (OS + process code)         |
 *               | .rodata                               |
 *               | .data                                 |
 *               | .bss                                  |
 *   0x82010000  +---------------------------------------+
 *               | OS stack  (8 KB, grows down)          |
 *   0x82012000  +---------------------------------------+  <-- __os_stack_top
 *               | P1 stack  (4 KB, grows down)          |
 *   0x82013000  +---------------------------------------+  <-- __p1_stack_top
 *               | P2 stack  (4 KB, grows down)          |
 *   0x82014000  +---------------------------------------+  <-- __p2_stack_top
 * ============================================================================= */

ENTRY(_start)

MEMORY
{
    OS_CODE  (rwx) : ORIGIN = 0x82000000, LENGTH = 64K
    OS_STACK (rw)  : ORIGIN = 0x82010000, LENGTH = 8K
    P1_STACK (rw)  : ORIGIN = 0x82012000, LENGTH = 4K
    P2_STACK (rw)  : ORIGIN = 0x82013000, LENGTH = 4K
}

SECTIONS
{
    .vectors 0x82000000 :
    {
        KEEP(*(.vectors))
    } > OS_CODE

    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } > OS_CODE

    .data :
    {
        *(.data)
        *(.data.*)
    } > OS_CODE

    .bss :
    {
        __bss_start__ = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end__ = .;
    } > OS_CODE

    /* OS stack */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __os_stack_bottom = .;
        . += 0x2000;
        __os_stack_top = .;
    } > OS_STACK

    /* P1 stack — grows down from __p1_stack_top */
    .p1_stack (NOLOAD) :
    {
        . = ALIGN(8);
        __p1_stack_bottom = .;
        . += 0x1000;
        __p1_stack_top = .;
    } > P1_STACK

    /* P2 stack — grows down from __p2_stack_top */
    .p2_stack (NOLOAD) :
    {
        . = ALIGN(8);
        __p2_stack_bottom = .;
        . += 0x1000;
        __p2_stack_top = .;
    } > P2_STACK

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
