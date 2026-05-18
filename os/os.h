/* =============================================================================
 * os.h — Shared OS header
 *
 * saved_spsr, launch_first_task(), svc_dispatch()
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
 * Globals compartidos entre root.s y el scheduler
 *
 * TRAP FRAME — llenados por root.s al entrar a IRQ o SVC:
 * ------------------------------------------------------------------------- */
extern unsigned int saved_regs[13];  /* R0-R12 del proceso interrumpido      */
extern unsigned int saved_lr;        /* LR_irq o LR_svc (return address)     */
extern unsigned int saved_svc_sp;    /* SP_usr del proceso (vía SYS mode)    */
extern unsigned int saved_svc_lr;    /* LR_usr del proceso (vía SYS mode)    */
extern unsigned int saved_spsr;      /* CPSR del proceso (USR=0x10) — NUEVO  */
 
/* ---------------------------------------------------------------------------
 * UART helpers 
 * ------------------------------------------------------------------------- */
void uart_putc(char c);
void uart_puts(const char *s);
void uart_put_uint(unsigned int n);

/* Trazas MODE_SWITCH — llamadas desde root.s (PID vía g_current_pid) */
void trace_initial_launch(void);
void trace_user_to_kernel_irq(void);
void trace_kernel_to_user_dispatch(void);
void trace_user_to_kernel_svc(void);
void trace_kernel_to_user_svc(void);
 
/* ---------------------------------------------------------------------------
 * Funciones del kernel
 * ------------------------------------------------------------------------- */
 
/* Handler del timer */
void timer_irq_handler(void);
 
/* Transición kernel → primer proceso en USR mode
 * Llamada desde main() después de scheduler_init() y cpu_irq_enable().
 * No retorna. */
void launch_first_task(void);
 
/* Dispatcher de syscalls
 * Lee saved_regs[0] como ID, ejecuta la syscall, escribe resultado
 * en saved_regs[0] y actualiza todos los saved_* con el siguiente proceso. */
void svc_dispatch(void);

/* Busy-wait delay for ~1 second (1 GHz) */
void delay_1s(void);
 
/* ---------------------------------------------------------------------------
 * Estados de proceso
 * ------------------------------------------------------------------------- */
typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_BLOCKED = 2
} proc_state_t;
 
/* ---------------------------------------------------------------------------
 * Memory map — usado por linker scripts y scheduler
 * ------------------------------------------------------------------------- */
#define OS_BASE         0x82000000U
#define OS_STACK_TOP    0x82012000U
 
#define P1_BASE         0x82100000U
#define P1_STACK_TOP    0x82112000U
 
#define P2_BASE         0x82200000U
#define P2_STACK_TOP    0x82212000U
 
#endif /* OS_H */