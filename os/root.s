@ =============================================================================
@ root.s — Bare-metal ARM Cortex-A8 entry point
@ BeagleBone Black (AM335x)
@
@ FASE 2 — Execution Modes and Syscalls
@ Dev A responsibilities:
@   [Fase 1 - sin cambios]
@   - Set CPU to System mode
@   - Set initial OS stack pointer
@   - Clear .bss section
@   - Set up ARM vector table via VBAR
@   - Jump to C main()
@
@   [Fase 2 - NUEVO]
@   - launch_first_task(): transición kernel → USR para el primer proceso
@   - irq_handler actualizado: guarda/restaura SP_usr via SYS mode + SPSR
@   - svc_handler real: guarda contexto, llama svc_dispatch(), retorna a USR
@   - 5 trazas MODE_SWITCH obligatorias (sección 3.7 del PDF)
@
@ =============================================================================
@ TRAP FRAME CONTRACT (acordado Dev A ↔ Dev B)
@
@   Globals que root.s llena al entrar a cualquier excepción:
@     saved_regs[0..12]  = R0-R12 del proceso interrumpido
@     saved_lr           = LR_irq (IRQ) o LR_svc (SVC) — return address
@     saved_svc_sp       = SP_usr del proceso (leído vía SYS mode)
@     saved_svc_lr       = LR_usr del proceso (leído vía SYS mode)
@     saved_spsr         = CPSR del proceso cuando fue interrumpido
@                          (para tasks en USR = 0x10)
@
@   Globals que Dev B (scheduler_tick / svc_dispatch) debe escribir
@   antes de retornar al handler, con los valores del SIGUIENTE proceso:
@     saved_regs[0..12]  ← R0-R12 del siguiente proceso
@     saved_lr           ← PC_siguiente + 4
@     saved_svc_sp       ← SP_usr del siguiente proceso
@     saved_svc_lr       ← LR_usr del siguiente proceso
@     saved_spsr         ← CPSR del siguiente proceso (0x10 para USR)
@
@   En SVC (SYS_YIELD), Dev B además escribe saved_regs[0] = 0 (rc=0).
@ =============================================================================
 
    .syntax unified
    .arch   armv7-a
    .arm
 
@ -----------------------------------------------------------------------------
@ ARM mode constants
@ -----------------------------------------------------------------------------
    .equ MODE_USR,  0x10        @ User mode
    .equ MODE_IRQ,  0x12        @ IRQ mode
    .equ MODE_SVC,  0x13        @ Supervisor mode
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
@ Sin cambios respecto a Fase 1.
@ -----------------------------------------------------------------------------
    .section .text
    .global reset_handler
reset_handler:
 
    @ 1. CPU a System mode, IRQ+FIQ deshabilitados durante init
    msr cpsr_c, #(MODE_SYS | IRQ_BIT | FIQ_BIT)
 
    @ 2. Stack pointer del OS → tope de región OS stack (0x82012000)
    ldr sp, =__os_stack_top
 
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
 
@ =============================================================================
@ launch_first_task — NUEVO Fase 2 (Sección 3.4 del PDF)
@
@ Llamada desde main() en os.c DESPUÉS de scheduler_init() y cpu_irq_enable().
@ Construye el contexto del primer proceso (P1) y salta a él en modo USR.
@
@ Mecanismo:
@   1. Escribir SP_usr de P1 en el banco USR (vía SYS mode)
@   2. Cargar SPSR_svc = 0x10 (USR mode, IRQs on)
@   3. Emitir traza MODE_SWITCH #1
@   4. movs pc, lr  →  PC = entry P1, CPSR = SPSR = USR mode
@
@ Precondición: Dev B ya llamó scheduler_init() que llenó:
@   saved_svc_sp = P1_STACK_TOP (0x82112000)
@   saved_lr     = P1_BASE + 4  (0x82100004)
@   saved_spsr   = 0x10         (USR mode)
@ =============================================================================
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
 
    @ --- Paso 3: traza MODE_SWITCH #1 (obligatoria, sección 3.7 fila 1) ---
    ldr  r0, =str_initial_launch
    bl   uart_puts
 
    @ --- Paso 4: cargar LR con entry point de P1 y saltar a USR ---
    @ saved_lr = P1_BASE + 4 = 0x82100004
    @ movs pc, lr  hace:  PC = LR,  CPSR = SPSR  → entramos a USR
    ldr  lr, =saved_lr
    ldr  lr, [lr]                    @ LR = P1_BASE + 4
    sub  lr, lr, #4                  @ LR = P1_BASE (corregir offset)
    movs pc, lr                      @ ← salto a USR, nunca retorna
 
@ =============================================================================
@ irq_handler — MODIFICADO Fase 2 (Sección 3.5 del PDF)
@
@ Cambios respecto a Fase 1:
@   1. Guarda SPSR_irq (= CPSR del proceso interrumpido) en saved_spsr
@   2. Usa SYS mode (no SVC) para leer SP_usr y LR_usr del proceso
@   3. Emite trazas USER_TO_KERNEL y KERNEL_TO_USER
@   4. Al restaurar: escribe saved_spsr en SPSR_irq ANTES de subs pc,lr,#4
@      → el CPU restaura CPSR = USR al retornar
@   5. Restaura SP_usr/LR_usr vía SYS mode (no SVC)
@ =============================================================================
    .global irq_handler
irq_handler:
 
    @ --- Guardar R0-R12 en saved_regs[0..12] ---
    push {r0}
    ldr  r0, =saved_regs
    pop  {r1}
    str  r1, [r0, #0]               @ saved_regs[0] = R0 original
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
 
    @ --- Guardar LR_irq (= PC_interrumpido + 4) ---
    ldr  r1, =saved_lr
    str  lr, [r1]
 
    @ --- NUEVO: guardar SPSR_irq = CPSR del proceso interrumpido ---
    @ Cuando el CPU entra a IRQ mode, copia automáticamente
    @ el CPSR del proceso → SPSR_irq. Para tasks USR, vale 0x10.
    mrs  r1, spsr
    ldr  r2, =saved_spsr
    str  r1, [r2]                   @ saved_spsr = CPSR del proceso (0x10 para USR)
 
    @ --- MODIFICADO: usar SYS mode para leer SP_usr y LR_usr ---
    @ Fase 1 usaba SVC mode, pero el proceso ahora corre en USR.
    @ SYS mode (0x1F) comparte los bancos de registros con USR,
    @ así que SP en SYS mode = SP_usr del proceso interrumpido.
    mrs  r2, cpsr                   @ guardar CPSR IRQ mode
    bic  r3, r2, #0x1F
    orr  r3, r3, #(MODE_SYS | IRQ_BIT)
    msr  cpsr_c, r3                 @ entrar a SYS mode (IRQs off)
    ldr  r4, =saved_svc_sp
    str  sp, [r4]                   @ saved_svc_sp = SP_usr del proceso
    ldr  r4, =saved_svc_lr
    str  lr, [r4]                   @ saved_svc_lr = LR_usr del proceso
    msr  cpsr_c, r2                 @ volver a IRQ mode
 
    @ --- Traza MODE_SWITCH #2: USER_TO_KERNEL (obligatoria, fila 2) ---
    ldr  r0, =str_user_to_kernel_irq
    bl   uart_puts
 
    @ --- Llamar al handler C: ack timer + INTC + scheduler_tick ---
    bl   timer_irq_handler
 
    @ --- Traza MODE_SWITCH #3: KERNEL_TO_USER (obligatoria, fila 3) ---
    ldr  r0, =str_kernel_to_user_dispatch
    bl   uart_puts
 
    @ --- NUEVO: cargar SPSR_irq con el CPSR del SIGUIENTE proceso ---
    @ Dev B escribió saved_spsr = 0x10 (USR) del proceso a restaurar.
    @ Al hacer subs pc,lr,#4, el CPU restaura CPSR = SPSR → llega a USR.
    ldr  r1, =saved_spsr
    ldr  r1, [r1]
    msr  spsr_cxsf, r1              @ SPSR_irq = 0x10 (USR del siguiente proceso)
 
    @ --- MODIFICADO: restaurar SP_usr y LR_usr vía SYS mode ---
    mrs  r2, cpsr
    bic  r3, r2, #0x1F
    orr  r3, r3, #(MODE_SYS | IRQ_BIT)
    msr  cpsr_c, r3                 @ SYS mode
    ldr  r4, =saved_svc_sp
    ldr  sp, [r4]                   @ SP_usr = saved_svc_sp del siguiente
    ldr  r4, =saved_svc_lr
    ldr  lr, [r4]                   @ LR_usr = saved_svc_lr del siguiente
    msr  cpsr_c, r2                 @ volver a IRQ mode
 
    @ --- Restaurar R0-R12 ---
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
    ldr  r0,  [r0, #0]             @ R0 último
 
    @ --- Retorno desde IRQ → USR ---
    @ subs pc, lr, #4:
    @   PC   = LR_irq - 4  = instrucción interrumpida
    @   CPSR = SPSR_irq    = 0x10 (USR) → el proceso retorna a USR
    subs pc, lr, #4
 
@ =============================================================================
@ svc_handler — NUEVO Fase 2, reemplaza stub (Sección 3.6 / 4.2 del PDF)
@
@ Activado cuando un proceso en USR ejecuta "svc #0".
@ El CPU automáticamente:
@   - Copia CPSR → SPSR_svc
@   - Guarda PC+4 → LR_svc
@   - Cambia a SVC mode
@
@ Este handler:
@   1. Guarda contexto igual que irq_handler (mismo trap frame)
@   2. Emite traza USER_TO_KERNEL reason=syscall
@   3. Llama svc_dispatch() en C (Dev B)
@   4. Emite traza KERNEL_TO_USER reason=syscall_return
@   5. Restaura contexto del proceso elegido por el scheduler
@   6. Retorna a USR con movs pc, lr
@ =============================================================================
    .global svc_handler
svc_handler:
 
    @ --- Guardar R0-R12 (mismo orden que irq_handler) ---
    @ R0 contiene el syscall ID (SYS_YIELD = 0) puesto por el task
    push {r0}
    ldr  r0, =saved_regs
    pop  {r1}
    str  r1, [r0, #0]               @ saved_regs[0] = syscall ID (r0 del task)
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
 
    @ --- Traza MODE_SWITCH #4: USER_TO_KERNEL syscall (obligatoria, fila 4) ---
    ldr  r0, =str_user_to_kernel_svc
    bl   uart_puts
 
    @ --- Llamar al dispatcher C (Dev B implementa svc_dispatch) ---
    @ svc_dispatch() lee saved_regs[0] para saber el ID (0 = SYS_YIELD),
    @ ejecuta el scheduler, y escribe los globals del proceso siguiente
    @ incluyendo saved_regs[0] = 0 (rc de retorno para el task).
    bl   svc_dispatch
 
    @ --- Traza MODE_SWITCH #5: KERNEL_TO_USER syscall_return (obligatoria, fila 5) ---
    ldr  r0, =str_kernel_to_user_svc
    bl   uart_puts
 
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
    ldr  lr,  [lr]                  @ LR = return address post-svc
    ldr  r0,  [r0, #0]             @ R0 = rc (0 = OK) — último
 
    @ --- Retorno desde SVC → USR ---
    @ movs pc, lr:
    @   PC   = LR_svc        = instrucción después del "svc #0"
    @   CPSR = SPSR_svc      = 0x10 (USR) → el task retorna a USR
    movs pc, lr
 
@ -----------------------------------------------------------------------------
@ Stub handlers — loops infinitos para excepciones no manejadas
@ -----------------------------------------------------------------------------
undef_handler:
    b undef_handler
 
prefetch_handler:
    b prefetch_handler
 
data_handler:
    b data_handler
 
fiq_handler:
    b fiq_handler
 
@ -----------------------------------------------------------------------------
@ Strings de trazas MODE_SWITCH (Sección 3.7 del PDF, filas 1-5 obligatorias)
@ Colocados en .text para que el linker los incluya en el binario del OS.
@ -----------------------------------------------------------------------------
    .section .text
str_initial_launch:
    .asciz "MODE_SWITCH KERNEL_TO_USER pid=1 reason=initial_launch\r\n"
 
str_user_to_kernel_irq:
    .asciz "MODE_SWITCH USER_TO_KERNEL pid=? reason=timer_irq\r\n"
 
str_kernel_to_user_dispatch:
    .asciz "MODE_SWITCH KERNEL_TO_USER pid=? reason=dispatch\r\n"
 
str_user_to_kernel_svc:
    .asciz "MODE_SWITCH USER_TO_KERNEL pid=? reason=syscall id=0\r\n"
 
str_kernel_to_user_svc:
    .asciz "MODE_SWITCH KERNEL_TO_USER pid=? reason=syscall_return id=0 rc=0\r\n"
 
@ End of root.s