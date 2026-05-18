#pragma once
#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------
 * Syscall identifiers
 * ------------------------------------------------------------------ */
enum { SYS_YIELD = 0, SYS_PRINT_CHAR = 1, SYS_PRINT_INT = 2 };

/* ------------------------------------------------------------------
 * syscall3
 *
 * Convención de registros:
 *   r0 : ID de la syscall (entrada) / valor de retorno (salida)
 *   r1–r3 : argumentos (todos 0 para SYS_YIELD)
 *   svc #0 : instrucción de trap
 * ------------------------------------------------------------------ */
static inline int32_t syscall3(uint32_t id, uint32_t a1, uint32_t a2, uint32_t a3)
{
    register uint32_t r0 asm("r0") = id;
    register uint32_t r1 asm("r1") = a1;
    register uint32_t r2 asm("r2") = a2;
    register uint32_t r3 asm("r3") = a3;
    asm volatile(
        "svc #0"
        : "=r"(r0)
        : "0"(r0), "r"(r1), "r"(r2), "r"(r3)
        : "memory"
    );
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

/* ------------------------------------------------------------------
 * sys_print_char — imprime un carácter (SYS_PRINT_CHAR, ID 1)
 *
 * Argumentos:
 *   r1 : carácter a imprimir
 *
 * Retorna:
 *   0  : éxito
 *  -1  : error
 * ------------------------------------------------------------------ */
static inline int32_t sys_print_char(char c)
{
    return syscall3(SYS_PRINT_CHAR, (uint32_t)c, 0, 0);
}

/* ------------------------------------------------------------------
 * sys_print_int — imprime un entero (SYS_PRINT_INT, ID 2)
 *
 * Argumentos:
 *   r1 : entero a imprimir
 *
 * Retorna:
 *   0  : éxito
 *  -1  : error
 * ------------------------------------------------------------------ */
static inline int32_t sys_print_int(int value)
{
    return syscall3(SYS_PRINT_INT, (uint32_t)value, 0, 0);
}
