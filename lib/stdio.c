#include "stdio.h"

typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

#define UART0_BASE      0x44E09000U
#define UART_THR        0x00
#define UART_LSR        0x14
#define UART_LSR_THRE   (1 << 5)

#define REG(base, offset) (*((volatile unsigned int *)((base) + (offset))))

static inline unsigned int irq_save_disable(void)
{
    unsigned int cpsr;
    __asm__ volatile (
        "mrs %0, cpsr      \n"
        "orr r1, %0, #0x80 \n"
        "msr cpsr_c, r1    \n"
        : "=r"(cpsr)
        :
        : "r1", "memory"
    );
    return cpsr;
}

static inline void irq_restore(unsigned int cpsr)
{
    __asm__ volatile (
        "msr cpsr_c, %0\n"
        :
        : "r"(cpsr)
        : "memory"
    );
}

void uart_putc(char c)
{
    while (!(REG(UART0_BASE, UART_LSR) & UART_LSR_THRE))
        ;
    REG(UART0_BASE, UART_THR) = (unsigned int)c;
}

void uart_puts(const char *s)
{
    while (s && *s)
        uart_putc(*s++);
}

static char *append_char(char *dst, char c, char *end)
{
    if (dst < end) *dst++ = c;
    return dst;
}

static char *append_str(char *dst, const char *src, char *end)
{
    if (!src) src = "(null)";
    while (*src && dst < end) *dst++ = *src++;
    return dst;
}

static char *append_uint(char *dst, unsigned int v, char *end)
{
    char tmp[11];
    int ti = 0;

    if (v == 0) {
        if (dst < end) *dst++ = '0';
        return dst;
    }

    while (v != 0 && ti < (int)sizeof(tmp)) {
        tmp[ti++] = (char)('0' + (v % 10));
        v /= 10;
    }
    while (ti-- > 0 && dst < end) *dst++ = tmp[ti];
    return dst;
}

static char *append_int(char *dst, int v, char *end)
{
    if (v < 0) {
        if (dst < end) *dst++ = '-';
        unsigned int u = (unsigned int)(-(v + 1)) + 1u;
        return append_uint(dst, u, end);
    } else {
        return append_uint(dst, (unsigned int)v, end);
    }
}

void PRINT(const char *fmt, ...)
{
    char out[192];
    char *p = out;
    char *end = out + sizeof(out) - 1;

    va_list ap;
    va_start(ap, fmt);

    const char *s = fmt;
    while (*s != '\0' && p < end) {
        if (*s != '%') {
            *p++ = *s++;
            continue;
        }

        s++;
        if (*s == '\0') break;

        switch (*s) {
        case 'd': {
            int v = va_arg(ap, int);
            p = append_int(p, v, end);
            break;
        }
        case 'u': {
            unsigned int v = va_arg(ap, unsigned int);
            p = append_uint(p, v, end);
            break;
        }
        case 'c': {
            int c = va_arg(ap, int);
            p = append_char(p, (char)c, end);
            break;
        }
        case 's': {
            const char *str = va_arg(ap, const char *);
            p = append_str(p, str, end);
            break;
        }
        case '%': {
            p = append_char(p, '%', end);
            break;
        }
        default: {
            p = append_char(p, '%', end);
            p = append_char(p, *s, end);
            break;
        }
        }
        s++;
    }

    *p = '\0';
    va_end(ap);

    unsigned int flags = irq_save_disable();
    uart_puts(out);
    irq_restore(flags);
}

void print_uint(unsigned int value)
{
    char buf[16];
    char *p = buf;
    char *end = buf + sizeof(buf) - 1;
    p = append_uint(p, value, end);
    *p = '\0';
    uart_puts(buf);
}

void print_int(int value)
{
    char buf[24];
    char *p = buf;
    char *end = buf + sizeof(buf) - 1;
    p = append_int(p, value, end);
    *p = '\0';
    uart_puts(buf);
}

void print_hex(unsigned int value)
{
    const char hex[] = "0123456789ABCDEF";
    char out[11];
    char *p = out;
    char *end = out + sizeof(out) - 1;

    p = append_char(p, '0', end);
    p = append_char(p, 'x', end);
    for (int i = 7; i >= 0 && p < end; --i) {
        unsigned int nib = (value >> (i * 4)) & 0xF;
        p = append_char(p, hex[nib], end);
    }
    *p = '\0';
    uart_puts(out);
}
