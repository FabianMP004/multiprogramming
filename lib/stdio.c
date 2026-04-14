/* ============================================================
 * lib/stdio.c
 * Autor : Dev B
 * Fase  : 1
 *
 * Implementación de las funciones de formato numérico.
 * uart_putc() y uart_puts() las provee Dev A en OS/os.c.
 * ============================================================ */

#include "stdio.h"

/* ============================================================
 * print_uint()
 *   Imprime un entero sin signo en decimal.
 *
 * Algoritmo:
 *   No se puede imprimir de izquierda a derecha directamente
 *   porque no sabemos cuántos dígitos hay.  En cambio:
 *     1. Extraer dígitos de derecha a izquierda con % 10
 *        y guardarlos en un buffer temporal.
 *     2. Imprimir el buffer al revés.
 *
 *   Ejemplo con value = 345:
 *     345 % 10 = 5  → buf[0] = '5',  value = 34
 *      34 % 10 = 4  → buf[1] = '4',  value = 3
 *       3 % 10 = 3  → buf[2] = '3',  value = 0
 *     Imprimir: buf[2] buf[1] buf[0] → "345"
 * ============================================================ */
void print_uint(unsigned int value)
{
    /* Buffer para los dígitos.
     * Un unsigned int tiene máximo 10 dígitos decimales. */
    char buf[10];
    int  i = 0;

    /* Caso especial: el valor es exactamente 0 */
    if (value == 0) {
        uart_putc('0');
        return;
    }

    /* Extraer dígitos de derecha a izquierda */
    while (value > 0) {
        buf[i] = '0' + (char)(value % 10);
        i++;
        value = value / 10;
    }

    /* Imprimir los dígitos de derecha a izquierda del buffer
     * (que es de izquierda a derecha del número) */
    while (i > 0) {
        i--;
        uart_putc(buf[i]);
    }
}

/* ============================================================
 * print_int()
 *   Imprime un entero con signo en decimal.
 *
 *   Si es negativo imprime '-' y luego el valor absoluto.
 *   El cast especial maneja el caso de INT_MIN (-2147483648)
 *   que no tiene representación positiva en int.
 * ============================================================ */
void print_int(int value)
{
    if (value < 0) {
        uart_putc('-');
        /* Convertir a unsigned evitando overflow con INT_MIN */
        print_uint((unsigned int)(-(value + 1)) + 1u);
    } else {
        print_uint((unsigned int)value);
    }
}

/* ============================================================
 * print_hex()
 *   Imprime un entero sin signo en hexadecimal.
 *   Siempre imprime 8 dígitos con prefijo "0x".
 *
 * Algoritmo:
 *   Un unsigned int tiene 32 bits = 8 nibbles (4 bits c/u).
 *   Extraer cada nibble de más significativo a menos:
 *     nibble 7 = bits 31-28
 *     nibble 6 = bits 27-24
 *     ...
 *     nibble 0 = bits 3-0
 *
 *   Ejemplo con value = 0x402F0400:
 *     nibble 7 = 4 → '4'
 *     nibble 6 = 0 → '0'
 *     nibble 5 = 2 → '2'
 *     nibble 4 = F → 'F'
 *     nibble 3 = 0 → '0'
 *     nibble 2 = 4 → '4'
 *     nibble 1 = 0 → '0'
 *     nibble 0 = 0 → '0'
 *     Resultado: "0x402F0400"
 * ============================================================ */
void print_hex(unsigned int value)
{
    /* Tabla de conversión de nibble a carácter hex */
    const char hex_chars[] = "0123456789ABCDEF";
    int i;

    /* Prefijo hexadecimal */
    uart_putc('0');
    uart_putc('x');

    /* Imprimir 8 nibbles del más significativo al menos */
    for (i = 7; i >= 0; i--) {
        unsigned int nibble = (value >> (i * 4)) & 0xF;
        uart_putc(hex_chars[nibble]);
    }
}
