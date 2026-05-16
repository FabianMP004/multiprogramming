/* =============================================================================
 * p2/main.c  —  User Task B (Fase 2)
 *
 * Corre en modo USR (ARM unprivileged).
 * Espejo de p1/main.c con su propio contador independiente.
 *
 * NOTA: este archivo reemplaza al p2/main.c de la Fase 1.
 * ============================================================================= */

#include <stdint.h>

/* ------------------------------------------------------------------
 * Syscall inline (Sección 4.7 del proyecto)
 * ------------------------------------------------------------------ */
enum { SYS_YIELD = 0 };

static inline int32_t syscall3(uint32_t id,
                                uint32_t a1, uint32_t a2, uint32_t a3)
{
    register uint32_t r0 asm("r0") = id;
    register uint32_t r1 asm("r1") = a1;
    register uint32_t r2 asm("r2") = a2;
    register uint32_t r3 asm("r3") = a3;
    asm volatile("svc #0\n"
                 : "+r"(r0)
                 : "r"(r1), "r"(r2), "r"(r3)
                 : "memory");
    return (int32_t)r0;
}

static inline int32_t sys_yield(void)
{
    return syscall3(SYS_YIELD, 0, 0, 0);
}

/* ------------------------------------------------------------------
 * Contador observable — independiente del de Task A
 * ------------------------------------------------------------------ */
volatile uint32_t g_b_yields;

int main(void)
{
    for (;;) {
        g_b_yields++;
        (void)sys_yield();
    }
}
