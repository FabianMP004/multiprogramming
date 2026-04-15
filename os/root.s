@ =============================================================================
@ root.s — Bare-metal ARM Cortex-A8 entry point
@ BeagleBone Black (AM335x)
@
@ Dev A responsibilities:
@ - Set CPU to System mode
@ - Set initial OS stack pointer
@ - Clear .bss section
@ - Set up ARM vector table via VBAR
@ - IRQ handler (Fase 3 - Context Switch completo)
@ - Jump to C main()
@ =============================================================================
    .syntax unified
    .arch armv7-a
    .arm

@ -----------------------------------------------------------------------------
@ ARM mode constants
@ -----------------------------------------------------------------------------
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
    ldr pc, =irq_handler      @ <-- Timer IRQ lands here
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

    @ Clear .bss
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
@ IRQ Handler - FASE 3 (Context Switch completo)
@ =============================================================================
    .global irq_handler
irq_handler:
    sub     lr, lr, #4                  @ PC correcto = LR_irq - 4
    stmfd   sp!, {r0-r12, lr}           @ Guardar R0-R12 + LR_irq

    @ Guardar SPSR, SP_svc y LR_svc
    mrs     r0, spsr
    msr     cpsr_c, #0xD3               @ SVC mode + IRQs disabled
    mov     r1, sp                      @ SP del proceso (SVC)
    mov     r2, lr                      @ LR del proceso
    msr     cpsr_c, #0xD2               @ volver a IRQ mode

    stmfd   sp!, {r0, r1, r2}           @ SPSR, SP_svc, LR_svc

    @ Llamar al handler en C
    bl      timer_irq_handler

    @ Restaurar contexto del siguiente proceso
    ldmfd   sp!, {r0, r1, r2}           @ SPSR, SP_svc, LR_svc
    msr     spsr_cxsf, r0
    msr     cpsr_c, #0xD3               @ SVC mode
    mov     sp, r1
    mov     lr, r2
    msr     cpsr_c, #0xD2               @ IRQ mode

    ldmfd   sp!, {r0-r12, lr}
    subs    pc, lr, #4                  @ ¡Regresar al siguiente proceso!
@ =============================================================================

@ -----------------------------------------------------------------------------
@ Stub handlers (sin cambios)
@ -----------------------------------------------------------------------------
undef_handler:
    b undef_handler
swi_handler:
    b swi_handler
prefetch_handler:
    b prefetch_handler
data_handler:
    b data_handler
fiq_handler:
    b fiq_handlers