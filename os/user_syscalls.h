#pragma once
#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------
 * Syscall identifiers  (Sección 4.4 del proyecto)
 * ------------------------------------------------------------------ */
enum { SYS_YIELD = 0 };

/* ------------------------------------------------------------------
 * syscall3 — wrapper genérico de 3 argumentos
 *
 * Convención de registros (Sección 4.3):
 *   r0 : ID de la syscall (entrada) / valor de retorno (salida)
 *   r1–r3 : argumentos (todos 0 para SYS_YIELD)
 *   svc #0 : instrucción de trap
 * ------------------------------------------------------------------ */
static inline int32_t syscall3(uint32_t id,
                                uint32_t a1,
                                uint32_t a2,
                                uint32_t a3)
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

/* ------------------------------------------------------------------
 * sys_yield — cede la CPU voluntariamente (SYS_YIELD, ID 0)
 *
 * Retorna:
 *   0  : éxito (el scheduler eligió otra tarea y volvió aquí)
 *  -1  : ID desconocido (no debería ocurrir con esta llamada)
 * ------------------------------------------------------------------ */
static inline int32_t sys_yield(void)
{
    return syscall3(SYS_YIELD, 0, 0, 0);
}
