#ifndef STDIO_H
#define STDIO_H

#ifdef __cplusplus
extern "C" {
#endif

void PRINT(const char *fmt, ...);

#if defined(__GNUC__) || defined(__clang__)
#define PRINTLN(fmt, ...) do { PRINT(fmt, ##__VA_ARGS__); PRINT("\r\n"); } while (0)
#else
#define PRINTLN(fmt, ...) do { PRINT(fmt, __VA_ARGS__); PRINT("\r\n"); } while (0)
#endif


void print_uint(unsigned int value);
void print_int(int value);
void print_hex(unsigned int value);

#ifdef __cplusplus
}
#endif

#endif /* STDIO_H */
