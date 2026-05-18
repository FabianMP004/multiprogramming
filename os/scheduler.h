#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "os.h"

#define PID_OS     0
#define PID_P1     1
#define PID_P2     2
#define NUM_PROCS  3

/* Proceso actual (1=P1, 2=P2) — trazas MODE_SWITCH y depuración */
extern unsigned int g_current_pid;

typedef struct {
    unsigned int pid;
    unsigned int pc;
    unsigned int sp;
    unsigned int lr;
    unsigned int spsr;
    unsigned int r[13];
    proc_state_t state;
} pcb_t;

/* Inicializa PCBs (P1/P2) */
void scheduler_init(void);

/* Devuelve PCB actual (útil para debugging) */
pcb_t *scheduler_current(void);

/* Called from the IRQ timer handler: do save/restore + select next */
void scheduler_tick(void);

/* Pre-load saved_* for launch_first_task() / early IRQ */
void scheduler_start_first(void);

/* Called from svc_handler in root.s */
void svc_dispatch(void);

#endif /* SCHEDULER_H */
