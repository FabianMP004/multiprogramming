/* =============================================================================
 * os.h — Shared OS header
 *
 * Included by:
 *   - os.c          (defines the globals)
 *   - Dev B's scheduler/PCB code (reads/writes globals)
 *   - Dev B's stdio.c / string.c (uses uart_putc / uart_puts)
 *
 * Dev A defines everything in this file.
 * Dev B should NOT modify os.h — only add their own header (e.g. pcb.h).
 * ============================================================================= */


#ifndef OS_H
#define OS_H

/* ---------------------------------------------------------------------------
 * Globals shared between root.s IRQ handler and the C scheduler (Dev B)
 *
 * root.s saves/restores these directly by symbol name.
 * Dev B's timer_irq_handler() reads them (to save current PCB) and writes
 * them (to set up the next PCB to be restored).
 * ------------------------------------------------------------------------- */
extern unsigned int saved_regs[13];  /* R0-R12 of interrupted process        */
extern unsigned int saved_lr;        /* LR_irq (= interrupted PC + 4)        */
extern unsigned int saved_svc_sp;    /* SVC stack pointer of interrupted proc */
extern unsigned int saved_svc_lr;    /* SVC link register of interrupted proc */

/* ---------------------------------------------------------------------------
 * UART helpers (implemented in os.c, used by Dev B's stdio.c)
 * ------------------------------------------------------------------------- */
void uart_putc(char c);
void uart_puts(const char *s);

/* ---------------------------------------------------------------------------
 * Timer IRQ handler — declared here, STUB in os.c, IMPLEMENTED by Dev B
 * Dev B replaces the body of this function (in os.c or their own .c file
 * that they link in place of the stub).
 * ------------------------------------------------------------------------- */
void timer_irq_handler(void);

/* ---------------------------------------------------------------------------
 * Process state enum — used by Dev B in pcb.h / scheduler
 * Defined here so it is available to everyone.
 * ------------------------------------------------------------------------- */
typedef enum {
    PROC_READY   = 0,
    PROC_RUNNING = 1,
    PROC_BLOCKED = 2
} proc_state_t;

/* ---------------------------------------------------------------------------
 * Memory map constants — used by linker scripts and Dev B's PCB init
 * ------------------------------------------------------------------------- */
#define OS_BASE         0x82000000U
#define OS_STACK_TOP    0x82012000U   /* top of 8 KB OS stack                */

#define P1_BASE         0x82100000U
#define P1_STACK_TOP    0x82112000U   /* top of 8 KB P1 stack                */

#define P2_BASE         0x82200000U
#define P2_STACK_TOP    0x82212000U   /* top of 8 KB P2 stack                */

#endif /* OS_H */
