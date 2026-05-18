@ =============================================================================
@ root.s — Bare-metal ARM Cortex-A8 entry point
@ BeagleBone Black (AM335x)
@ =============================================================================

    .syntax unified
    .arch   armv7-a
    .arm
 
@ -----------------------------------------------------------------------------
@ ARM mode constants
@ -----------------------------------------------------------------------------
    .equ MODE_USR,  0x10        @ User mode
    .equ MODE_FIQ,  0x11        @ FIQ mode
    .equ MODE_IRQ,  0x12        @ IRQ mode
    .equ MODE_SVC,  0x13        @ Supervisor mode
    .equ MODE_ABT,  0x17        @ Abort mode (Data/Prefetch abort)
    .equ MODE_UND,  0x1B        @ Undefined Instruction mode
    .equ MODE_SYS,  0x1F        @ System mode (comparte bancos con USR)
    .equ IRQ_BIT,   0x80        @ I-bit en CPSR (deshabilita IRQ si = 1)
    .equ FIQ_BIT,   0x40        @ F-bit en CPSR
 
@ -----------------------------------------------------------------------------
@ Vector table — placed at .vectors section (linked to 0x82000000)
@ VBAR = 0x82000000, entonces el CPU usa esta tabla para todas las excepciones.
@ -----------------------------------------------------------------------------
    .section .vectors, "ax"
    .global _start
_start:
    ldr pc, =reset_handler      @ 0x00 Reset
    ldr pc, =undef_handler      @ 0x04 Undefined instruction
    ldr pc, =svc_handler        @ 0x08 SWI/SVC  ← FASE 2: handler real
    ldr pc, =prefetch_handler   @ 0x0C Prefetch abort
    ldr pc, =data_handler       @ 0x10 Data abort
    nop                         @ 0x14 Reserved
    ldr pc, =irq_handler        @ 0x18 IRQ  ← timer interrupt
    ldr pc, =fiq_handler        @ 0x1C FIQ
 
@ -----------------------------------------------------------------------------
@ Reset handler — primer código que ejecuta el CPU
@ -----------------------------------------------------------------------------
    .section .text
    .global reset_handler
reset_handler:
 
    @ 1. CPU a System mode, IRQ+FIQ deshabilitados durante init
    msr cpsr_c, #(MODE_SYS | IRQ_BIT | FIQ_BIT)
 
    @ 2. Stack pointer del OS → tope de región OS stack (0x82012000)
    ldr sp, =__os_stack_top
    
    @ 2a. Initialize IRQ mode stack to dedicated IRQ stack region
    msr cpsr_c, #(MODE_IRQ | IRQ_BIT | FIQ_BIT)
    ldr sp, =__irq_stack_top
    
    @ 2b. Initialize SVC mode stack to main OS stack region
    msr cpsr_c, #(MODE_SVC | IRQ_BIT | FIQ_BIT)
    ldr sp, =__os_stack_top
    
    @ 2c. Initialize ABT mode stack to main OS stack region
    msr cpsr_c, #(MODE_ABT | IRQ_BIT | FIQ_BIT)
    ldr sp, =__os_stack_top
    
    @ 2d. Initialize UND mode stack to main OS stack region
    msr cpsr_c, #(MODE_UND | IRQ_BIT | FIQ_BIT)
    ldr sp, =__os_stack_top
    
    @ 2e. Initialize FIQ mode stack to main OS stack region
    msr cpsr_c, #(MODE_FIQ | IRQ_BIT | FIQ_BIT)
    ldr sp, =__os_stack_top
    
    @ 2f. Return to SYS mode for remainder of init
    msr cpsr_c, #(MODE_SYS | IRQ_BIT | FIQ_BIT)
 
    @ 3. VBAR apunta a nuestra tabla de vectores en 0x82000000
    ldr r0, =_start
    mcr p15, 0, r0, c12, c0, 0
    isb
 
    @ 4. Limpiar .bss a cero
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
bss_loop:
    cmp r0, r1
    strlt r2, [r0], #4
    blt bss_loop
 
    @ 5. Memory barriers
    dsb
    isb
 
    @ 6. Saltar a C main()
    bl main
 
halt:
    b halt

    .global launch_first_task
launch_first_task:

    @ --- Paso 1: cargar SP_usr del primer proceso ---
    @ SYS mode (0x1F) comparte los registros banqueados con USR,
    @ así que escribir SP en SYS mode es exactamente escribir SP_usr.
    mrs  r0, cpsr                    @ guardar CPSR actual (SVC mode)
    bic  r1, r0, #0x1F
    orr  r1, r1, #(MODE_SYS | IRQ_BIT)
    msr  cpsr_c, r1                  @ entrar a SYS mode (IRQs off)
    ldr  r2, =saved_svc_sp
    ldr  sp, [r2]                    @ SP_usr = saved_svc_sp (P1_STACK_TOP)
    msr  cpsr_c, r0                  @ volver a SVC mode

    @ --- Paso 2: construir SPSR con USR mode (0x10, IRQs habilitadas) ---
    ldr  r1, =saved_spsr
    ldr  r1, [r1]                    @ r1 = 0x10 (puesto por scheduler_init)
    msr  spsr_cxsf, r1               @ SPSR_svc = 0x10

    @ --- Traza #1 ya impresa desde main() antes de launch_first_task ---

    @ --- Paso 3: cargar LR con entry point de P1 y saltar a USR ---
    @ saved_lr = P1_BASE = 0x82100000
    @ movs pc, lr  hace:  PC = LR,  CPSR = SPSR  → entramos a USR
    ldr  lr, =saved_lr
    ldr  lr, [lr]                    @ LR = P1_BASE
    movs pc, lr                      @ ← salto a USR, nunca retorna
 
    .global irq_handler
irq_handler:
 
    @ --- Guardar R0-R12 en saved_regs[0..12] ---
    @ Layout: R0@0, R1@4, R2@8, R3@12, ..., R12@48  (13 words = 52 bytes)
    @ R1 debe guardarse ANTES de hacer pop {r1}, porque pop sobreescribe r1 con R0.
    push {r0}
    ldr  r0, =saved_regs
    str  r1, [r0, #4]               @ saved_regs[1] = R1 original (antes de perderlo)
    pop  {r1}                       @ ahora r1 = R0 original
    str  r1, [r0, #0]               @ saved_regs[0] = R0 original
    str  r2, [r0, #8]
    str  r3, [r0, #12]
    str  r4, [r0, #16]
    str  r5, [r0, #20]
    str  r6, [r0, #24]
    str  r7, [r0, #28]
    str  r8, [r0, #32]
    str  r9, [r0, #36]
    str  r10,[r0, #40]
    str  r11,[r0, #44]
    str  r12,[r0, #48]
 
    @ --- Guardar LR_irq (= PC_interrumpido + 4) ---
    ldr  r1, =saved_lr
    str  lr, [r1]
 
    @ --- Guardar SPSR_irq = CPSR del proceso interrumpido ---
    @ Cuando el CPU entra a IRQ mode, copia automáticamente
    @ el CPSR del proceso → SPSR_irq. Para tasks USR, vale 0x10.
    mrs  r1, spsr
    ldr  r2, =saved_spsr
    str  r1, [r2]                   @ saved_spsr = CPSR del proceso (0x10 para USR)
 
    mrs  r2, cpsr                   @ guardar CPSR IRQ mode
    bic  r3, r2, #0x1F
    orr  r3, r3, #(MODE_SYS | IRQ_BIT)
    msr  cpsr_c, r3                 @ entrar a SYS mode (IRQs off)
    ldr  r4, =saved_svc_sp
    str  sp, [r4]                   @ saved_svc_sp = SP_usr del proceso
    ldr  r4, =saved_svc_lr
    str  lr, [r4]                   @ saved_svc_lr = LR_usr del proceso
    msr  cpsr_c, r2                 @ volver a IRQ mode
 
    @ --- Timer C handler: ACK timer + scheduler_tick() ---
    bl   timer_irq_handler
    
    ldr  r1, =saved_spsr
    ldr  r1, [r1]
    msr  spsr_cxsf, r1              @ SPSR_irq = 0x10 (USR del siguiente proceso)
    
    @ --- Restaurar SP_usr y LR_usr vía SYS mode ---
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
    ldr  lr,  [lr]
    ldr  r0,  [r0, #0]              @ R0 último
 
    @ --- Retorno desde IRQ → USR ---
    @ subs pc, lr, #4:
    @   PC   = LR_irq - 4  = instrucción interrumpida
    @   CPSR = SPSR_irq    = 0x10 (USR) → el proceso retorna a USR
    subs pc, lr, #4
 
    .global svc_handler
svc_handler:
 
    @ --- Guardar R0-R12 (mismo orden que irq_handler) ---
    @ Layout: R0@0, R1@4, R2@8, R3@12, ..., R12@48  (13 words = 52 bytes)
    @ R0 contiene el syscall ID (SYS_YIELD = 0) puesto por el task
    @ R1 debe guardarse ANTES de hacer pop {r1}, porque pop sobreescribe r1 con R0.
    push {r0}
    ldr  r0, =saved_regs
    str  r1, [r0, #4]               @ saved_regs[1] = R1 original (antes de perderlo)
    pop  {r1}                       @ ahora r1 = R0 original (syscall ID)
    str  r1, [r0, #0]               @ saved_regs[0] = syscall ID (r0 del task)
    str  r2, [r0, #8]
    str  r3, [r0, #12]
    str  r4, [r0, #16]
    str  r5, [r0, #20]
    str  r6, [r0, #24]
    str  r7, [r0, #28]
    str  r8, [r0, #32]
    str  r9, [r0, #36]
    str  r10,[r0, #40]
    str  r11,[r0, #44]
    str  r12,[r0, #48]
 
    @ --- Guardar LR_svc (= dirección de retorno, instrucción post-svc) ---
    ldr  r1, =saved_lr
    str  lr, [r1]
 
    @ --- Guardar SPSR_svc (= CPSR del proceso cuando ejecutó svc) ---
    @ Para tasks en USR, vale 0x10.
    mrs  r1, spsr
    ldr  r2, =saved_spsr
    str  r1, [r2]
 
    @ --- Guardar SP_usr y LR_usr vía SYS mode ---
    @ En SVC mode el SP_svc es el del kernel, no el del proceso.
    @ SYS mode comparte bancos con USR, así leemos SP_usr real.
    mrs  r2, cpsr                   @ guardar CPSR SVC mode
    bic  r3, r2, #0x1F
    orr  r3, r3, #(MODE_SYS | IRQ_BIT)
    msr  cpsr_c, r3                 @ SYS mode
    ldr  r4, =saved_svc_sp
    str  sp, [r4]                   @ saved_svc_sp = SP_usr del proceso
    ldr  r4, =saved_svc_lr
    str  lr, [r4]                   @ saved_svc_lr = LR_usr del proceso
    msr  cpsr_c, r2                 @ volver a SVC mode
    
    @ --- Syscall handler: dispatcher in C ---
    bl   svc_dispatch
 
    @ --- Restaurar SPSR_svc con el CPSR del proceso a retornar ---
    @ Dev B puso saved_spsr = 0x10 del proceso seleccionado.
    ldr  r1, =saved_spsr
    ldr  r1, [r1]
    msr  spsr_cxsf, r1              @ SPSR_svc = 0x10 (USR)
 
    @ --- Restaurar SP_usr y LR_usr del proceso a retornar ---
    mrs  r2, cpsr
    bic  r3, r2, #0x1F
    orr  r3, r3, #(MODE_SYS | IRQ_BIT)
    msr  cpsr_c, r3                 @ SYS mode
    ldr  r4, =saved_svc_sp
    ldr  sp, [r4]
    ldr  r4, =saved_svc_lr
    ldr  lr, [r4]
    msr  cpsr_c, r2                 @ volver a SVC mode
 
    @ --- Restaurar R0-R12 del proceso a retornar ---
    @ R0 = saved_regs[0] = rc puesto por Dev B (0 para SYS_YIELD OK)
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
    ldr  lr,  [lr]                  @ LR = PC del siguiente proceso
    ldr  r0,  [r0, #0]             @ R0 = rc (0 = OK) — último
 
    @ --- Retorno desde SVC → USR ---
    @ movs pc, lr:
    @   PC   = LR_svc        = dirección de retorno del proceso seleccionado
    @   CPSR = SPSR_svc      = 0x10 (USR) → el task retorna a USR
    movs pc, lr
 
@ -----------------------------------------------------------------------------
@ Visible crash handlers for unexpected exceptions
@ These print error messages and then halt
@ -----------------------------------------------------------------------------
undef_handler:
    push {r0, r1, lr}
    ldr  r0, =str_crash_undef
    bl   uart_puts
    pop  {r0, r1, lr}
    b    undef_handler
 
prefetch_handler:
    push {r0, r1, lr}
    ldr  r0, =str_crash_prefetch
    bl   uart_puts
    pop  {r0, r1, lr}
    b    prefetch_handler
 
data_handler:
    push {r0, r1, lr}
    ldr  r0, =str_crash_data
    bl   uart_puts
    pop  {r0, r1, lr}
    b    data_handler
 
fiq_handler:
    push {r0, r1, lr}
    ldr  r0, =str_crash_fiq
    bl   uart_puts
    pop  {r0, r1, lr}
    b    fiq_handler
 
@ String definitions for trace messages and crash handlers
.section .data
str_crash_undef:
    .asciz "[CRASH] Undefined Instruction\r\n"

str_crash_prefetch:
    .asciz "[CRASH] Prefetch Abort\r\n"

str_crash_data:
    .asciz "[CRASH] Data Abort\r\n"

str_crash_fiq:
    .asciz "[CRASH] FIQ Interrupt\r\n"

.section .text

@ End of root.s
