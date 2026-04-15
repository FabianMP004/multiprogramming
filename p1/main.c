/* =============================================================================
 * p1/main.c  —  User Process 1 (Dev C)
 * Imprime dígitos 0-9 en bucle infinito
 * ============================================================================= */

#include "os.h"
#include "stdio.h"

int main(void)
{
    int i = 0;

    while (1) {
        PRINT("----From P1: %d\r\n", i);
        i = (i + 1) % 10;

        /* Delay para que se vea el interleaving con P2 */
        for (volatile int d = 0; d < 200000; ++d);
    }

    return 0;   /* Nunca llega aquí */
}