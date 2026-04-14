/* ============================================================
 * OS/os_main.c
 * Autor : Dev B
 * Fase  : 1
 *
 * Punto de entrada C del sistema operativo.
 * Dev A lo llama desde root.s después de inicializar
 * el hardware (WDT, UART, DMTimer2, INTC).
 *
 * Secuencia:
 *   1. Imprime banner de boot por UART.
 *   2. Llama scheduler_init().
 *   3. Entra en idle loop con WFI.
 * ============================================================ */

#include "os.h"
#include "scheduler.h"
#include "../lib/stdio.h"

/* ============================================================
 * os_main()
 *   Llamada por root.s (Dev A) luego de inicializar HW.
 * ============================================================ */
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

    uart_puts("\r\n");
    uart_puts("[OS] Scheduler listo.\r\n");
    uart_puts("[OS] Entrando en idle loop...\r\n");
    uart_puts("\r\n");

    while (1) {
        __asm__ volatile ("wfi");
    }
}