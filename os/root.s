@ =============================================================================
@ root.s — Bare-metal ARM Cortex-A8 entry point
@ BeagleBone Black (AM335x)
@ =============================================================================
    .syntax unified
    .arch armv7-a
    .arm

    .equ MODE_IRQ, 0x12
    .equ MODE_SVC, 0x13
    .equ MODE_SYS, 0x1F
    .equ IRQ_BIT,  0x80
    .equ FIQ_BIT,  0x40

@ -----------------------------------------------------------------------------
@ Vector table
@ -----------------------------------------------------------------------------
    .section .vectors, "ax"
    .global _start
_start:
    ldr pc, =reset_handler
    ldr pc, =undef_handler
    ldr pc, =swi_handler
    ldr pc, =prefetch_handler
    ldr pc, =data_handler
    nop
    ldr pc, =irq_handler
    ldr pc, =fiq_handler

@ -----------------------------------------------------------------------------
@ Reset handler
@ -----------------------------------------------------------------------------
    .section .text
    .global reset_handler
reset_handler:
    msr cpsr_c, #(MODE_SYS | IRQ_BIT | FIQ_BIT)
    ldr sp, =__os_stack_top
    ldr r0, =_start
    mcr p15, 0, r0, c12, c0, 0
    isb

    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
bss_loop:
    cmp r0, r1
    strlt r2, [r0], #4
    blt bss_loop

    dsb
    isb
    bl main

halt:
    b halt

@ =============================================================================
@ Context-save variables — allocated here, exported for scheduler.c
@ =============================================================================
    .section .bss
    .global saved_regs
    .global saved_lr
    .global saved_svc_sp
    .global saved_svc_lr
    .balign 4
saved_regs:   .space 52    @ r0-r12 (13 words × 4)
saved_lr:     .space 4     @ LR_irq = interrupted PC + 4
saved_svc_sp: .space 4     @ SP of interrupted process (SVC bank)
saved_svc_lr: .space 4     @ LR of interrupted process (SVC bank)

@ =============================================================================
@ IRQ Handler — saves context into saved_*, calls scheduler_tick(), restores
@ =============================================================================
    .section .text
    .global irq_handler
irq_handler:
    @ Correct LR_irq to exact interrupted PC
    sub     lr, lr, #4

    @ Push r0-r12 + LR onto IRQ stack temporarily
    stmfd   sp!, {r0-r12, lr}

    @ Peek into SVC bank for SP_svc / LR_svc
    mrs     r0, spsr
    msr     cpsr_c, #0xD3           @ switch to SVC, IRQs off
    mov     r1, sp
    mov     r2, lr
    msr     cpsr_c, #0xD2           @ back to IRQ

    @ Persist to saved_* globals
    ldr     r3, =saved_regs
    ldmfd   sp, {r4-r12}            @ peek (not pop) r1-r12 from stack
    @ Actually copy r0-r12 from stack to saved_regs
    add     r3, r3, #0              @ r3 = &saved_regs[0]
    ldmfd   sp!, {r4}               @ pop r0
    str     r4, [r3], #4            @ saved_regs[0] = r0
    ldmfd   sp!, {r4-r12}           @ pop r1-r9
    stmia   r3!, {r4-r12}           @ saved_regs[1-9]
    ldmfd   sp!, {r4-r6}            @ pop r10, r11, r12
    stmia   r3!, {r4-r6}            @ saved_regs[10-12]
    ldmfd   sp!, {r4}               @ pop original LR (= interrupted PC)
    ldr     r3, =saved_lr
    str     r4, [r3]
    ldr     r3, =saved_svc_sp
    str     r1, [r3]
    ldr     r3, =saved_svc_lr
    str     r2, [r3]

    @ Call C IRQ handler to ACK timer/INTC and pick next process
    bl      timer_irq_handler

    @ Restore next process's SVC SP/LR
    ldr     r1, =saved_svc_sp
    ldr     r1, [r1]
    ldr     r2, =saved_svc_lr
    ldr     r2, [r2]
    msr     cpsr_c, #0xD3           @ SVC mode
    mov     sp, r1
    mov     lr, r2
    msr     cpsr_c, #0xD2           @ IRQ mode

    @ Restore r0-r12
    ldr     r0, =saved_regs
    ldmia   r0, {r0-r12}

    @ Keep tasks in SVC mode: scheduler saves/restores SVC SP/LR per process.
    @ Returning to USER here would switch to banked SP_usr (never initialized),
    @ which can corrupt stack and eventually freeze the system.
    mov     r0, #0x13
    msr     spsr_cxsf, r0

    @ Return to next process PC (saved_lr)
    ldr     lr, =saved_lr
    ldr     lr, [lr]
    movs    pc, lr

@ -----------------------------------------------------------------------------
@ Stub handlers
@ -----------------------------------------------------------------------------
undef_handler:    b undef_handler
swi_handler:      b swi_handler
prefetch_handler: b prefetch_handler
data_handler:     b data_handler
fiq_handler:      b fiq_handler
