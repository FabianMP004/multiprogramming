#include "os.h"
#include "stdio.h"

/* main.c P1 */

int main(void)
{
    int i = 0;

    while (1) {
        PRINT("----From P1: %d\r\n", i);
        i = (i + 1) % 10;
        for (volatile int d = 0; d < 200000; ++d);
    }

    return 0;
}
