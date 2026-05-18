#include "os.h"
#include "scheduler.h"
#include "../lib/stdio.h"

void os_main(void)
{
    uart_puts("\r\n");
    uart_puts("========================================\r\n");
    uart_puts("  BeagleBone Bare-Metal Multiprogramming\r\n");
    uart_puts("========================================\r\n");
    uart_puts("[OS] Hardware init completo (Dev A)\r\n");
    uart_puts("[OS] Iniciando scheduler    (Dev B)\r\n");
    uart_puts("\r\n");

    scheduler_init();
    scheduler_start_first();
    trace_initial_launch();

    uart_puts("\r\n");
    uart_puts("[OS] Scheduler listo.\r\n");
    uart_puts("[OS] Saltando a P1...\r\n");
    uart_puts("\r\n");

    launch_first_task();

    /* nunca llega aquí */
    while (1) {
        __asm__ volatile ("wfi");
    }
}