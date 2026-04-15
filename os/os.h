#ifndef OS_H
#define OS_H

/* --------------------------------------------------------------------------
 * Globals shared between root.s IRQ handler and the C scheduler
 *
 * root.s saves/restores these directly by symbol name.
 * timer_irq_handler() reads them (to save current PCB) and writes
 * them (to set up the next PCB to be restored).
 * -------------------------------------------------------------------------- */
extern unsigned int saved_regs[13];  /* R0-R12 of interrupted process         */
extern unsigned int saved_lr;        /* LR_irq (= interrupted PC + 4)         */
extern unsigned int saved_svc_sp;    /* SVC stack pointer of interrupted proc */
extern unsigned int saved_svc_lr;    /* SVC link register of interrupted proc */

/* ---------------------------------------------------------------------------
 * UART helpers (implemented in os.c, used by stdio.c)
 * --------------------------------------------------------------------------- */
void uart_putc(char c);
void uart_puts(const char *s);

/* ---------------------------------------------------------------------------
 * Timer IRQ handler — declared here, STUB in os.c
 * ------------------------------------------------------------------------- */
void timer_irq_handler(void);

/* ---------------------------------------------------------------------------
 * Process state enum — used in pcb.h / scheduler
 * ------------------------------------------------------------------------- */
typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_BLOCKED = 2
} proc_state_t;

/* ---------------------------------------------------------------------------
 * Memory map constants — used by linker scripts and PCB init
 * ------------------------------------------------------------------------- */
#define OS_BASE         0x82000000U
#define OS_STACK_TOP    0x82012000U   /* top of 8 KB OS stack                */

#define P1_BASE         0x82100000U
#define P1_STACK_TOP    0x82112000U   /* top of 8 KB P1 stack                */

#define P2_BASE         0x82200000U
#define P2_STACK_TOP    0x82212000U   /* top of 8 KB P2 stack                */

#endif /* OS_H */
