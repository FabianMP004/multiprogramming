/* os/scheduler.c — implementación mínima del scheduler (Dev B / integración Dev C)
 *
 * Provee:
 *  - scheduler_init()      : inicializa PCBs para P1/P2
 *  - scheduler_tick()      : llamada desde timer_irq_handler para hacer RR
 *  - scheduler_start_first(): opcional (prepara saved_* para primer proceso)
 *
 * Usa las variables globales declaradas en os/pcb.h:
 *   volatile pcb_t g_pcb[NUM_PROCS];
 *   volatile uint32_t g_current_pid;
 */

/* os/scheduler.c — implementación del scheduler y scheduler_tick */
#include "scheduler.h"
#include "os.h"

/* Usa variables estáticas internas para los PCBs */
static pcb_t procs[NUM_PROCS];
static uint32_t current_proc = PID_P1;

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

static void build_initial_frame(pcb_t *p, uint32_t entry, uint32_t stack_top, uint32_t pid)
{
    clear_pcb(p);
    p->pid = pid;
    p->pc  = entry;
    p->sp  = stack_top;
    p->lr  = 0;
    p->spsr = 0;
    p->state = PROC_READY;
}

void scheduler_init(void)
{
    build_initial_frame(&procs[PID_P1], P1_BASE, P1_STACK_TOP, PID_P1);
    build_initial_frame(&procs[PID_P2], P2_BASE, P2_STACK_TOP, PID_P2);
    current_proc = PID_P1;
    procs[PID_P1].state = PROC_RUNNING;
}

pcb_t *scheduler_current(void)
{
    return &procs[current_proc];
}

/* Preload saved_* so that a return from IRQ (if performed) would resume P1 */
void scheduler_start_first(void)
{
    for (int i = 0; i < 13; ++i)
        saved_regs[i] = procs[PID_P1].r[i];
    saved_lr = procs[PID_P1].pc + 4;
    saved_svc_sp = procs[PID_P1].sp;
    saved_svc_lr = procs[PID_P1].lr;
    procs[PID_P1].state = PROC_RUNNING;
}

/* Called from timer handler after ACK. It saves current interrupted context
 * into the PCB for current_proc, picks the next proc (round-robin), and
 * writes next proc context back into saved_* so root.s will restore it.
 */
void scheduler_tick(void)
{
    uint32_t cur = current_proc;
    uint32_t next;

    /* Save interrupted registers into procs[cur] */
    for (int i = 0; i < 13; ++i)
        procs[cur].r[i] = saved_regs[i];

    /* saved_lr is LR_irq (= interrupted PC + 4) */
    procs[cur].pc = saved_lr - 4;
    procs[cur].sp = saved_svc_sp;
    procs[cur].lr = saved_svc_lr;
    procs[cur].state = PROC_READY;

    /* Round-Robin selection (only two user processes) */
    next = (cur == PID_P1) ? PID_P2 : PID_P1;
    current_proc = next;

    /* Restore next process into saved_* so root.s restore picks it up */
    for (int i = 0; i < 13; ++i)
        saved_regs[i] = procs[next].r[i];
    saved_lr = procs[next].pc + 4;
    saved_svc_sp = procs[next].sp;
    saved_svc_lr = procs[next].lr;
    procs[next].state = PROC_RUNNING;
}
