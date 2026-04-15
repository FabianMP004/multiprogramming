#include "os.h"
#include "stdio.h"

/* main.c P2 */

int main(void)
{
    char c = 'a';

    while (1) {
        PRINT("----From P2: %c\r\n", c);
        if (c >= 'z') c = 'a';
        else c++;

        for (volatile int d = 0; d < 200000; ++d);
    }

    return 0;
}
