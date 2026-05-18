#include "user_syscalls.h"

int main(void) {
    char c;
    c = 'a';  /* Inicialización explícita */
    for (;;) {
        sys_print_char(c);
        c = (c >= 'z') ? 'a' : (char)(c + 1);
        sys_yield();
    }
    return 0;
}