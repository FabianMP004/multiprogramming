/* os/processes.c — User process entry points */
#include "os.h"

void p1_entry(void)
{
    while (1) {
        uart_puts("[P1] running\r\n");
        /* busy wait */
        for (volatile int i = 0; i < 1000000; i++);
    }
}

void p2_entry(void)
{
    while (1) {
        uart_puts("[P2] running\r\n");
        for (volatile int i = 0; i < 1000000; i++);
    }
}