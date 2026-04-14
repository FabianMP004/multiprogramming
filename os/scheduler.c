/* ============================================================
 * OS/scheduler.c
 * Autor : Dev B
 * Fase  : 1
 *
 * Contiene:
 *   - scheduler_init()       → inicializa PCBs de P1 y P2
 *   - scheduler_current()    → retorna PCB activo
 *
 * NO contiene timer_irq_handler() — eso es Fase 2.
 * ============================================================ */

#include "scheduler.h"
#include "os.h"

static pcb_t procs[NUM_PROCS];
static uint32_t current_proc = PID_P1;

static void clear_pcb(pcb_t *p)
{
    uint32_t i;

    p->pid = 0;
    p->pc = 0;
    p->sp = 0;
    p->lr = 0;
    p->spsr = 0;

    for (i = 0; i < 13; i++) {
        p->r[i] = 0;
    }

    p->state = PROC_READY;
}

static void build_initial_frame(pcb_t *p, uint32_t entry, uint32_t stack_top)
{
    uint32_t i;

    for (i = 0; i < 13; i++) {
        p->r[i] = 0;
    }

    p->pid = 0;
    p->pc = entry;
    p->sp = stack_top;
    p->lr = 0;
    p->spsr = 0;
    p->state = PROC_READY;
}

void scheduler_init(void)
{
    clear_pcb(&procs[PID_P1]);
    clear_pcb(&procs[PID_P2]);

    procs[PID_P1].pid = PID_P1;
    build_initial_frame(&procs[PID_P1], P1_BASE, P1_STACK_TOP);

    procs[PID_P2].pid = PID_P2;
    build_initial_frame(&procs[PID_P2], P2_BASE, P2_STACK_TOP);

    current_proc = PID_P1;
    procs[PID_P1].state = PROC_RUNNING;
}

pcb_t *scheduler_current(void)
{
    return &procs[current_proc];
}