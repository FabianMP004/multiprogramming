
Copiar

/* =============================================================================
 * os.h — Shared OS header
 *
 * FASE 2: agregados saved_spsr, launch_first_task(), svc_dispatch()
 *
 * Incluido por:
 *   - os.c          (define los globals)
 *   - root.s        (usa los symbols directamente)
 *   - scheduler.c   (Dev B lee/escribe los globals)
 *   - stdio.c       (Dev B usa uart_putc/uart_puts)
 * ============================================================================= */
 
#ifndef OS_H
#define OS_H
 
/* ---------------------------------------------------------------------------
 * Globals compartidos entre root.s y el scheduler de Dev B
 *
 * TRAP FRAME — llenados por root.s al entrar a IRQ o SVC:
 * ------------------------------------------------------------------------- */
extern unsigned int saved_regs[13];  /* R0-R12 del proceso interrumpido      */
extern unsigned int saved_lr;        /* LR_irq o LR_svc (return address)     */
extern unsigned int saved_svc_sp;    /* SP_usr del proceso (vía SYS mode)    */
extern unsigned int saved_svc_lr;    /* LR_usr del proceso (vía SYS mode)    */
extern unsigned int saved_spsr;      /* CPSR del proceso (USR=0x10) — NUEVO  */
 
/* ---------------------------------------------------------------------------
 * UART helpers (implementados en os.c)
 * ------------------------------------------------------------------------- */
void uart_putc(char c);
void uart_puts(const char *s);
 
/* ---------------------------------------------------------------------------
 * Funciones del kernel (implementadas en root.s / os.c)
 * ------------------------------------------------------------------------- */
 
/* Fase 1: handler del timer — Dev B implementa el cuerpo */
void timer_irq_handler(void);
 
/* Fase 2 NUEVO: transición kernel → primer proceso en USR mode
 * Llamada desde main() después de scheduler_init() y cpu_irq_enable().
 * No retorna. */
void launch_first_task(void);
 
/* Fase 2 NUEVO: dispatcher de syscalls — Dev B implementa
 * Lee saved_regs[0] como ID, ejecuta la syscall, escribe resultado
 * en saved_regs[0] y actualiza todos los saved_* con el siguiente proceso. */
void svc_dispatch(void);
 
/* ---------------------------------------------------------------------------
 * Estados de proceso — usados por Dev B en el scheduler
 * ------------------------------------------------------------------------- */
typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_BLOCKED = 2
} proc_state_t;
 
/* ---------------------------------------------------------------------------
 * Memory map — usado por linker scripts y scheduler (Dev B)
 * ------------------------------------------------------------------------- */
#define OS_BASE         0x82000000U
#define OS_STACK_TOP    0x82012000U
 
#define P1_BASE         0x82100000U
#define P1_STACK_TOP    0x82112000U
 
#define P2_BASE         0x82200000U
#define P2_STACK_TOP    0x82212000U
 
#endif /* OS_H */