#include "os.h"
#include "scheduler.h"
#include "../lib/stdio.h"


void os_main(void)
{
    uart_puts("\r\n");
    uart_puts("========================================\r\n");
    uart_puts("  BeagleBone Bare-Metal Multiprogramming\r\n");
    uart_puts("========================================\r\n");
    uart_puts("[OS] Hardware init completo \r\n");
    uart_puts("[OS] Iniciando scheduler    \r\n");
    uart_puts("\r\n");

    scheduler_init();

    uart_puts("\r\n");
    uart_puts("[OS] Scheduler listo.\r\n");
    uart_puts("[OS] Entrando en idle loop...\r\n");
    uart_puts("\r\n");

    while (1) {
        __asm__ volatile ("wfi");
    }
}