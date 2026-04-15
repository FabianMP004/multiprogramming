/* =============================================================================
 * p2/main.c  —  User Process 2 (Dev C)
 * Imprime letras a-z en bucle infinito
 * ============================================================================= */

#include "os.h"
#include "stdio.h"

int main(void)
{
    char c = 'a';

    while (1) {
        PRINT("----From P2: %c\r\n", c);
        c = (c == 'z') ? 'a' : (c + 1);

        /* Delay para que se vea el interleaving con P1 */
        for (volatile int d = 0; d < 200000; ++d);
    }

    return 0;   /* Nunca llega aquí */
}