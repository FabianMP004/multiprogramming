/* os/scheduler.c — Phase 2 scheduler (USR mode, SYS_YIELD) */

#include "scheduler.h"
#include "os.h"

unsigned int saved_spsr = 0;
unsigned int g_current_pid = PID_P1;

static pcb_t procs[NUM_PROCS];

static void sys_yield_handler(void);

static void clear_pcb(pcb_t *p)
{
    p->pid = 0;
    p->pc = 0;
    p->sp = 0;
    p->lr = 0;
    p->spsr = 0;
    for (int i = 0; i < 13; ++i)
        p->r[i] = 0;
    p->state = PROC_READY;
}

static void build_initial_frame(pcb_t *p, unsigned int entry, unsigned int stack_top, unsigned int pid)
{
    clear_pcb(p);
    p->pid = pid;
    p->pc  = entry;
    p->sp  = stack_top;
    p->lr  = 0;
    p->spsr = 0x10;
    p->state = PROC_READY;
}

void scheduler_init(void)
{
    build_initial_frame(&procs[PID_P1], P1_BASE, P1_STACK_TOP, PID_P1);
    build_initial_frame(&procs[PID_P2], P2_BASE, P2_STACK_TOP, PID_P2);
    g_current_pid = PID_P1;
    procs[PID_P1].state = PROC_RUNNING;
}

pcb_t *scheduler_current(void)
{
    return &procs[g_current_pid];
}

void scheduler_start_first(void)
{
    int i;

    for (i = 0; i < 13; ++i)
        saved_regs[i] = procs[PID_P1].r[i];
    saved_lr     = procs[PID_P1].pc;  /* launch_first_task uses movs pc, lr (exact, not subs) */
    saved_svc_sp = procs[PID_P1].sp;
    saved_svc_lr = procs[PID_P1].lr;
    saved_spsr   = procs[PID_P1].spsr;
    procs[PID_P1].state = PROC_RUNNING;
}

void scheduler_tick(void)
{
    unsigned int cur = g_current_pid;
    unsigned int next;
    int i;


    for (i = 0; i < 13; ++i)
        procs[cur].r[i] = saved_regs[i];

    procs[cur].pc   = saved_lr - 4;
    procs[cur].sp   = saved_svc_sp;
    procs[cur].lr   = saved_svc_lr;
    procs[cur].spsr = saved_spsr;
    procs[cur].state = PROC_READY;

    next = (cur == PID_P1) ? PID_P2 : PID_P1;
    g_current_pid = next;

    for (i = 0; i < 13; ++i)
        saved_regs[i] = procs[next].r[i];
    saved_lr     = procs[next].pc + 4;
    saved_svc_sp = procs[next].sp;
    saved_svc_lr = procs[next].lr;
    saved_spsr   = procs[next].spsr;
    procs[next].state = PROC_RUNNING;

}

static void sys_yield_handler(void)
{
    unsigned int cur  = g_current_pid;
    unsigned int next = (cur == PID_P1) ? PID_P2 : PID_P1;
    int i;


    for (i = 0; i < 13; ++i)
        procs[cur].r[i] = saved_regs[i];
    procs[cur].pc   = saved_lr;
    procs[cur].sp   = saved_svc_sp;
    procs[cur].lr   = saved_svc_lr;
    procs[cur].spsr = saved_spsr;
    procs[cur].state = PROC_READY;

    g_current_pid = next;

    for (i = 0; i < 13; ++i)
        saved_regs[i] = procs[next].r[i];
    saved_lr     = procs[next].pc;
    saved_svc_sp = procs[next].sp;
    saved_svc_lr = procs[next].lr;
    saved_spsr   = procs[next].spsr;
    procs[next].state = PROC_RUNNING;

    procs[cur].r[0] = 0;

}

static void sys_print_char_handler(void)
{
    char c = (char)saved_regs[1];
    uart_puts("----From P2: ");
    uart_putc(c);
    uart_puts("\r\n");
    saved_regs[0] = 0;
}

static void sys_print_int_handler(void)
{
    int value = (int)saved_regs[1];
    uart_puts("----From P1: ");
    uart_put_uint((unsigned int)value);
    uart_puts("\r\n");
    saved_regs[0] = 0;
}

void svc_dispatch_body(void)
{
    unsigned int id = saved_regs[0];

    trace_user_to_kernel_svc();

    if (id == 0)
        sys_yield_handler();
    else if (id == 1)
        sys_print_char_handler();
    else if (id == 2)
        sys_print_int_handler();
    else
        saved_regs[0] = (unsigned int)-1;

    trace_kernel_to_user_svc();
}

__attribute__((naked))
void svc_dispatch(void)
{
    __asm__ volatile (
        "mrs  r1, cpsr           \n"
        "mrs  r0, cpsr           \n"
        "bic  r0, r0, #0x1F      \n"
        "orr  r0, r0, #0x13      \n"
        "msr  cpsr_c, r0         \n"
        "ldr  sp, =__os_stack_top\n"
        "msr  cpsr_c, r1         \n"
        "b    svc_dispatch_body  \n"
    );
}
