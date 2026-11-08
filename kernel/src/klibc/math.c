#include "klibc/math.h"

#include <limits.h>

int abs(int value)
{
    if (value == INT_MIN)
    {
        return INT_MAX;
    }
    return (value < 0) ? -value : value;
}