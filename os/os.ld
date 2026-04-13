/* =============================================================================
 * os.ld — Linker script for the OS image
 *
 * Memory layout:
 *   0x82000000  +---------------------------------------+
 *               | .vectors  (vector table, 32 bytes)    |
 *               | .text     (OS code)                   |
 *               | .rodata   (read-only data)            |
 *               | .data     (initialized globals)       |
 *               | .bss      (zero-initialized globals)  |
 *   0x82010000  +---------------------------------------+
 *               | OS stack  (8 KB, grows down)          |
 *   0x82012000  +---------------------------------------+  <-- __os_stack_top
 *
 * Total OS region: 72 KB (64 KB code+data + 8 KB stack)
 * ============================================================================= */

ENTRY(_start)

MEMORY
{
    OS_CODE (rwx) : ORIGIN = 0x82000000, LENGTH = 64K
    OS_STACK (rw) : ORIGIN = 0x82010000, LENGTH = 8K
}

SECTIONS
{
    /* --- Vector table must be at the very start of the image ---
     * VBAR points to 0x82000000, so the vector table must be first.       */
    .vectors 0x82000000 :
    {
        KEEP(*(.vectors))
    } > OS_CODE

    /* --- Code and read-only data --- */
    .text :
    {
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
    } > OS_CODE

    /* --- Initialized data ---
     * Loaded from flash/binary, copied to RAM by startup if needed.
     * On BeagleBone with U-Boot, everything is already in RAM.            */
    .data :
    {
        *(.data)
        *(.data.*)
    } > OS_CODE

    /* --- Zero-initialized data ---
     * root.s clears from __bss_start__ to __bss_end__ on startup.        */
    .bss :
    {
        __bss_start__ = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end__ = .;
    } > OS_CODE

    /* --- OS stack region ---
     * The stack grows DOWN from __os_stack_top.
     * root.s sets SP = __os_stack_top.                                    */
    .stack (NOLOAD) :
    {
        . = ALIGN(8);
        __os_stack_bottom = .;
        . += 0x2000;           /* 8 KB */
        __os_stack_top = .;
    } > OS_STACK

    /* Discard debug info to keep binary small */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
    }
}
