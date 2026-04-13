@ =============================================================================
@ root.s — Bare-metal ARM Cortex-A8 entry point
@ BeagleBone Black (AM335x)
@
@ Dev A responsibilities:
@   - Set CPU to System mode
@   - Set initial OS stack pointer
@   - Clear .bss section
@   - Set up ARM vector table via VBAR
@   - IRQ handler (saves R0-R12 + LR_irq, calls C handler, restores, returns)
@   - Jump to C main()
@ =============================================================================

    .syntax unified
    .arch   armv7-a
    .arm

@ -----------------------------------------------------------------------------
@ ARM mode constants
@ -----------------------------------------------------------------------------
    .equ MODE_IRQ,  0x12        @ IRQ mode
    .equ MODE_SVC,  0x13        @ Supervisor mode
    .equ MODE_SYS,  0x1F        @ System mode
    .equ IRQ_BIT,   0x80        @ I-bit in CPSR (disables IRQ when set)
    .equ FIQ_BIT,   0x40        @ F-bit in CPSR

@ -----------------------------------------------------------------------------
@ Vector table — placed at .vectors section (linked to 0x82000000)
@ The OS linker script places this section at the very start of the image.
@ VBAR is then set to 0x82000000 so the CPU uses this table.
@ -----------------------------------------------------------------------------
    .section .vectors, "ax"
    .global _start
_start:
    ldr pc, =reset_handler      @ 0x00 Reset
    ldr pc, =undef_handler      @ 0x04 Undefined instruction
    ldr pc, =swi_handler        @ 0x08 SWI / SVC
    ldr pc, =prefetch_handler   @ 0x0C Prefetch abort
    ldr pc, =data_handler       @ 0x10 Data abort
    nop                         @ 0x14 Reserved
    ldr pc, =irq_handler        @ 0x18 IRQ  <-- timer interrupt lands here
    ldr pc, =fiq_handler        @ 0x1C FIQ

@ -----------------------------------------------------------------------------
@ Reset handler — first code the CPU executes after reset / go command
@ -----------------------------------------------------------------------------
    .section .text
    .global reset_handler
reset_handler:

    @ 1. Set CPU to System mode, IRQ+FIQ disabled while we initialize
    msr cpsr_c, #(MODE_SYS | IRQ_BIT | FIQ_BIT)

    @ 2. Set OS stack pointer (top of OS stack region: 0x82010000)
    @    Stack grows downward, so we point to the top of the reserved region.
    ldr sp, =__os_stack_top

    @ 3. Set VBAR — point the CPU's vector base to our vector table
    ldr r0, =_start
    mcr p15, 0, r0, c12, c0, 0  @ Write to VBAR (CP15 c12)
    isb                          @ Instruction sync barrier

    @ 4. Clear .bss section to zero
    @    __bss_start__ and __bss_end__ are defined by the linker script.
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
bss_loop:
    cmp r0, r1
    strlt r2, [r0], #4
    blt bss_loop

    @ 5. Memory barriers — ensure all writes are visible before entering C
    dsb
    isb

    @ 6. Jump to C main
    bl main

    @ Should never return — halt if it does
halt:
    b halt

@ -----------------------------------------------------------------------------
@ IRQ Handler (Fase 2)
@
@ When a timer IRQ fires:
@   - CPU auto-saves CPSR → SPSR_irq, sets LR_irq = interrupted PC + 4 (ARM)
@   - CPU switches to IRQ mode
@
@ What we do here:
@   a) Save R0-R12 and LR_irq into global arrays (saved_regs[], saved_lr)
@   b) Call C function timer_irq_handler()
@   c) Restore R0-R12 and LR_irq from those same arrays
@   d) Return with: subs pc, lr, #4  (restores CPSR from SPSR_irq)
@
@ Note: saved_regs and saved_lr are defined in os.c (extern arrays).
@       The C handler reads/writes the PCB and updates saved_regs/saved_lr
@       so that on restore we land in the NEXT process, not the current one.
@ -----------------------------------------------------------------------------
    .global irq_handler
irq_handler:

    @ --- Save R0-R12 into saved_regs[0..12] ---
    @     We use R13 (SP_irq) as scratch — safe because we're in IRQ mode.
    push {r0}                        @ Temporarily save R0 on IRQ stack
    ldr  r0, =saved_regs             @ R0 = &saved_regs[0]
    pop  {r1}                        @ Recover original R0 into R1
    str  r1, [r0, #0]                @ saved_regs[0] = original R0
    str  r2, [r0, #4]
    str  r3, [r0, #8]
    str  r4, [r0, #12]
    str  r5, [r0, #16]
    str  r6, [r0, #20]
    str  r7, [r0, #24]
    str  r8, [r0, #28]
    str  r9, [r0, #32]
    str  r10,[r0, #36]
    str  r11,[r0, #40]
    str  r12,[r0, #44]

    @ --- Save LR_irq (= interrupted PC + 4 in ARM state) ---
    ldr  r1, =saved_lr
    str  lr, [r1]                    @ saved_lr = LR_irq

    @ --- Save SVC mode SP and LR ---
    @     The interrupted process was in SVC mode. We need to save its SP/LR.
    @     Temporarily switch to SVC with IRQs still disabled, read SP+LR, back.
    mrs  r2, cpsr                    @ Save current IRQ mode CPSR
    bic  r3, r2, #0x1F
    orr  r3, r3, #(MODE_SVC | IRQ_BIT)
    msr  cpsr_c, r3                  @ Switch to SVC mode (IRQs disabled)
    ldr  r4, =saved_svc_sp
    str  sp, [r4]                    @ saved_svc_sp = SVC SP
    ldr  r4, =saved_svc_lr
    str  lr, [r4]                    @ saved_svc_lr = SVC LR (R14_svc)
    msr  cpsr_c, r2                  @ Back to IRQ mode

    @ --- Call C timer handler ---
    @     timer_irq_handler() in os.c will:
    @       1. Ack timer + INTC
    @       2. Save current PCB from saved_regs/saved_lr/saved_svc_sp/saved_svc_lr
    @       3. Pick next process (Round-Robin) — Dev B implements this
    @       4. Load next PCB into saved_regs/saved_lr/saved_svc_sp/saved_svc_lr
    bl   timer_irq_handler

    @ --- Restore SVC mode SP and LR for the NEXT process ---
    mrs  r2, cpsr
    bic  r3, r2, #0x1F
    orr  r3, r3, #(MODE_SVC | IRQ_BIT)
    msr  cpsr_c, r3                  @ Switch to SVC mode
    ldr  r4, =saved_svc_sp
    ldr  sp, [r4]                    @ Restore SVC SP
    ldr  r4, =saved_svc_lr
    ldr  lr, [r4]                    @ Restore SVC LR
    msr  cpsr_c, r2                  @ Back to IRQ mode

    @ --- Restore R0-R12 from saved_regs ---
    ldr  r0, =saved_regs
    ldr  r1,  [r0, #4]
    ldr  r2,  [r0, #8]
    ldr  r3,  [r0, #12]
    ldr  r4,  [r0, #16]
    ldr  r5,  [r0, #20]
    ldr  r6,  [r0, #24]
    ldr  r7,  [r0, #28]
    ldr  r8,  [r0, #32]
    ldr  r9,  [r0, #36]
    ldr  r10, [r0, #40]
    ldr  r11, [r0, #44]
    ldr  r12, [r0, #48]
    ldr  lr,  =saved_lr
    ldr  lr,  [lr]                   @ LR_irq = saved_lr (set by C handler)
    ldr  r0,  [r0, #0]               @ Restore R0 last

    @ --- Return from IRQ ---
    @     subs pc, lr, #4:
    @       PC = LR_irq - 4  = address of interrupted instruction
    @       CPSR restored from SPSR_irq  (returns to SVC mode + process state)
    subs pc, lr, #4

@ -----------------------------------------------------------------------------
@ Stub handlers — trap unexpected exceptions with a simple infinite loop
@ (Add UART output here for debugging if needed)
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
    b fiq_handler

@ End of root.s
