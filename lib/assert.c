#include <stdio.h>
#include <stdbool.h>

void assert(bool pred)
{
    if (!pred)
    {
        printf("Assertion failure!\n");
    }
}
