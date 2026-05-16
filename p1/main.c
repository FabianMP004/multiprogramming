/* =============================================================================
 * p1/main.c  —  User Task A  (Fase 2: Execution Modes and Syscalls)
 *
 * Corre en modo USR (ARM unprivileged, CPSR[4:0] = 0x10).
 * Cede el CPU con sys_yield() → svc #0 → kernel → scheduler RR.
 * g_a_yields es volatile: observable desde el debugger sin optimizar.
 *
 * Reemplaza el p1/main.c de Fase 1 (que usaba PRINT + delays).
 * ============================================================================= */

#include "user_syscalls.h"   /* compilar con -I os para encontrar este header */

volatile uint32_t g_a_yields;

int main(void)
{
    for (;;) {
        g_a_yields++;
        (void)sys_yield();
    }
}
