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

void scheduler_init(void);
pcb_t *scheduler_current(void);

#endif /* SCHEDULER_H */