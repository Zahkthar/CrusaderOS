#include "klibc/math.h"

int abs(int value)
{
    return (value < 0) ? -value : value;
}