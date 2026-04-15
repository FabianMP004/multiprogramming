#include "scheduler.h"
#include "os.h"
#include <stdint.h>

static pcb_t procs[NUM_PROCS];
static uint32_t current_proc = PID_P1;

/* Clear a PCB */
static void clear_pcb(pcb_t *p)
{
    p->pid = 0;
    p->pc = 0;
    p->sp = 0;
    p->lr = 0;
    p->spsr = 0;
    for (int i = 0; i < 13; ++i) p->r[i] = 0;
    p->state = PROC_READY;
}

/* Build initial PCB frame for a user process */
static void build_initial_frame(pcb_t *p, uint32_t entry, uint32_t stack_top, uint32_t pid)
{
    clear_pcb(p);
    p->pid = pid;
    p->pc  = entry;
    p->sp  = stack_top;
    p->lr  = entry;   /* default LR_svc (unused initially) */
    p->spsr = 0;
    p->state = PROC_READY;

    /* Leave registers zeroed — the process will start with cleared registers */
}

/* Initialize PCBs for P1 and P2 */
void scheduler_init(void)
{
    build_initial_frame(&procs[PID_P1], P1_BASE, P1_STACK_TOP, PID_P1);
    build_initial_frame(&procs[PID_P2], P2_BASE, P2_STACK_TOP, PID_P2);

    current_proc = PID_P1;
    procs[PID_P1].state = PROC_RUNNING;
}

/* Returns pointer to current PCB */
pcb_t *scheduler_current(void)
{
    return &procs[current_proc];
}

/* Preload saved_* so that a restore will resume P1.
 * This sets saved_regs (R0-R12), saved_lr (LR_irq = PC + 4),
 * saved_svc_sp and saved_svc_lr for the first process.
 */
void scheduler_start_first(void)
{
    /* Ensure PCBs are initialized */
    if (procs[PID_P1].pc == 0) {
        /* If scheduler_init hasn't been called, do it */
        scheduler_init();
    }

    /* Ensure registers are zeroed for first run */
    for (int i = 0; i < 13; ++i)
        procs[PID_P1].r[i] = 0;

    /* Preload globals read by root.s restore sequence */
    for (int i = 0; i < 13; ++i)
        saved_regs[i] = procs[PID_P1].r[i];

    /* root.s stores/restores saved_lr as the exact resume PC */
    saved_lr = procs[PID_P1].pc;

    /* SVC-mode stack top and svc LR */
    saved_svc_sp = procs[PID_P1].sp;
    saved_svc_lr = procs[PID_P1].lr;

    procs[PID_P1].state = PROC_RUNNING;
    current_proc = PID_P1;
}

/* scheduler_tick: save current interrupted context into current PCB,
 * choose next process (RR) and restore its context into saved_* globals.
 *
 * Contract:
 *  - root.s IRQ handler saved R0-R12 into saved_regs and LR_irq into saved_lr
 *  - scheduler_tick should copy saved_* -> pcb[cur], then set saved_* from pcb[next]
 */
void scheduler_tick(void)
{
    uint32_t cur = current_proc;
    uint32_t next;

    /* Save interrupted registers into PCB for cur */
    for (int i = 0; i < 13; ++i)
        procs[cur].r[i] = saved_regs[i];

    /* root.s already converted LR_irq to exact resume PC */
    procs[cur].pc = saved_lr;
    procs[cur].sp = saved_svc_sp;
    procs[cur].lr = saved_svc_lr;
    procs[cur].state = PROC_READY;

    /* Select next process (only two user processes here) */
    next = (cur == PID_P1) ? PID_P2 : PID_P1;
    current_proc = next;

    /* Restore next process into saved_* so root.s will restore it */
    for (int i = 0; i < 13; ++i)
        saved_regs[i] = procs[next].r[i];

    saved_lr = procs[next].pc;
    saved_svc_sp = procs[next].sp;
    saved_svc_lr = procs[next].lr;
    procs[next].state = PROC_RUNNING;
}