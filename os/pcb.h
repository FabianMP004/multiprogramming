#ifndef PCB_H
#define PCB_H

#include <stdint.h>
#include "os.h"

#define PID_OS      0
#define PID_P1      1
#define PID_P2      2
#define NUM_PROCS   3

typedef struct {
    uint32_t pid;
    uint32_t pc;
    uint32_t sp;
    uint32_t lr;
    uint32_t spsr;
    uint32_t r[13];   /* R0-R12 */
    proc_state_t state;
} pcb_t;

extern volatile pcb_t g_pcb[NUM_PROCS];
extern volatile uint32_t g_current_pid;

void scheduler_init(void);
void scheduler_tick(void);
void scheduler_start_first(void);

#endif /* PCB_H */