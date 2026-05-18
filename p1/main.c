#include "user_syscalls.h"

int main(void) {
    int counter;
    counter = 0;  /* Inicialización explícita */
    for (;;) {
        sys_print_int(counter);
        counter++;
        if (counter > 9) {
            counter = 0;
        }
        sys_yield();
    }
    return 0;
}