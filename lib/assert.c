#include <stdio.h>
#include <stdbool.h>

void assert(bool pred, int id)
{
    if (!pred)
    {
        printf("Assertion %d failed!\n", id);
    }
}
