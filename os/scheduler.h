#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include "os.h"

#define PID_OS     0
#define PID_P1     1
#define PID_P2     2
#define NUM_PROCS  3

typedef struct {
    uint32_t pid;
    uint32_t pc;
    uint32_t sp;
    uint32_t lr;
    uint32_t spsr;
    uint32_t r[13];
    proc_state_t state;
} pcb_t;

/* Inicializa PCBs (P1/P2) */
void scheduler_init(void);

/* Devuelve PCB actual (útil para debugging) */
pcb_t *scheduler_current(void);

/* Called from the IRQ timer handler: do save/restore + select next */
void scheduler_tick(void);

/* Opcional: pre-load saved_* for first process so early IRQ returns into it */
void scheduler_start_first(void);

#endif /* SCHEDULER_H */
