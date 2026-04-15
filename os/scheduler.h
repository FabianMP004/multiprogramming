#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "os.h"

/* Process IDs */
#define PID_OS     0
#define PID_P1     1
#define PID_P2     2
#define NUM_PROCS  3

/* Minimal PCB structure used by the scheduler and OS */
typedef struct {
    uint32_t pid;
    uint32_t pc;        /* program counter (actual PC) */
    uint32_t sp;        /* SVC-mode stack pointer (top) */
    uint32_t lr;        /* saved LR (R14_svc) */
    uint32_t spsr;      /* saved SPSR (if needed) */
    uint32_t r[13];     /* R0-R12 */
    proc_state_t state;
} pcb_t;

/* Initialize PCBs for P1/P2 */
void scheduler_init(void);

/* Return pointer to current PCB (for OS debugging) */
pcb_t *scheduler_current(void);

/* Called from the IRQ handler: save current, select next, restore saved_* */
void scheduler_tick(void);

/* Prepare the first process context so a restore path will resume it.
 * This preloads saved_regs, saved_lr, saved_svc_sp, saved_svc_lr so that
 * root.s's restore sequence (subs pc, lr, #4) or the IRQ return path
 * will resume the selected process. Called before enabling IRQs.
 */
void scheduler_start_first(void);

#endif /* SCHEDULER_H */
