#ifndef STDIO_H
#define STDIO_H

/* ============================================================
 * lib/stdio.h
 * Autor : Dev B
 * Fase  : 1
 *
 * Librería de I/O mínima para bare-metal BeagleBone.
 * No usa printf, scanf ni ninguna función de libc.
 * Toda la salida va a UART0 mediante uart_putc() (Dev A).
 * ============================================================ */

/* ----------------------------------------------------------
 * uart_putc y uart_puts
 *   Implementadas en OS/os.c por Dev A.
 *   Declaradas aquí para que stdio.c y los procesos las usen
 *   sin necesidad de incluir os.h completo.
 * ---------------------------------------------------------- */
void uart_putc(char c);
void uart_puts(const char *s);

/* ----------------------------------------------------------
 * PRINT(str)
 *   Macro principal que Dev C usa en P1/main.c y P2/main.c.
 *   Ejemplo de uso:
 *       PRINT("Hola desde P1\n");
 * ---------------------------------------------------------- */
#define PRINT(str)      uart_puts(str)

/* ----------------------------------------------------------
 * PRINTLN(str)
 *   Como PRINT pero agrega salto de línea automático.
 *   Ejemplo:
 *       PRINTLN("Proceso 1 corriendo");
 *       → imprime "Proceso 1 corriendo\r\n"
 * ---------------------------------------------------------- */
#define PRINTLN(str)    do { uart_puts(str); uart_puts("\r\n"); } while(0)

/* ----------------------------------------------------------
 * Funciones de formato numérico
 *   Imprimen directamente a UART0, no retornan strings.
 * ---------------------------------------------------------- */

/* print_uint — imprime entero sin signo en decimal
 *   Ejemplo: print_uint(42)  → "42"
 *            print_uint(0)   → "0"            */
void print_uint(unsigned int value);

/* print_int — imprime entero con signo en decimal
 *   Ejemplo: print_int(-5)   → "-5"
 *            print_int(100)  → "100"          */
void print_int(int value);

/* print_hex — imprime entero en hexadecimal (8 dígitos)
 *   Ejemplo: print_hex(0xFF)         → "0x000000FF"
 *            print_hex(0x402F0400)   → "0x402F0400"  */
void print_hex(unsigned int value);

#endif /* STDIO_H */
