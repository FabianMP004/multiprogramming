#ifndef STDIO_H
#define STDIO_H

/* ============================================================
 * lib/stdio.h
 * Minimal stdio for the multiprogramming project.
 *
 * Purpose:
 *   - Provide a simple, lightweight PRINT(fmt, ...) function that
 *     forwards formatted output to UART via uart_puts()/uart_putc().
 *   - Keep the interface minimal so user processes (P1/P2) can use
 *     PRINT(...) without pulling in full libc.
 *
 * Notes:
 *   - uart_putc() / uart_puts() are implemented in OS/os.c (Dev A).
 *   - The implementation of PRINT(...) (formatting) is provided in
 *     lib/stdio.c (Dev B) and must be linked into P1/P2 images.
 * ============================================================ */

/* UART primitives (implemented in OS/os.c) */
void uart_putc(char c);
void uart_puts(const char *s);

/* Main printf-like API for user processes.
 * Usage:
 *   PRINT("Value=%d\r\n", x);
 */
void PRINT(const char *fmt, ...);

/* Convenience: PRINT with automatic CRLF appended.
 * Example:
 *   PRINTLN("Hello from P1");   -> prints "Hello from P1\r\n"
 *
 * NOTE: this macro uses a variadic macro extension to swallow the
 * optional arguments portably on GCC/Clang.
 */
#if defined(__GNUC__) || defined(__clang__)
#define PRINTLN(fmt, ...) do { PRINT(fmt, ##__VA_ARGS__); PRINT("\r\n"); } while (0)
#else
/* Fallback (may require at least one vararg on some compilers) */
#define PRINTLN(fmt, ...) do { PRINT(fmt, __VA_ARGS__); PRINT("\r\n"); } while (0)
#endif

/* Numeric helpers (direct UART output) */
void print_uint(unsigned int value);
void print_int(int value);
void print_hex(unsigned int value);

#endif /* STDIO_H */