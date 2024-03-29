#include "klibc/stdlib.h"

#include "klibc/math.h"
#include "klibc/string.h"

// +-----------------------------+
// | [NUMERIC <=> STR] FUNCTIONS |
// +-----------------------------+

char *itoa(int value, char *buffer, int base)
{
    if (base < 2 || base > 36)
    {
        return buffer;
    }

    // Writing of the absolute value in buffer
    int absValue = abs(value);
    int i = 0;

    while (absValue != 0)
    {
        int r = absValue % base;

        if (r >= 10)
        {
            buffer[i++] = 'A' + (r - 10);
        }
        else
        {
            buffer[i++] = '0' + r;
        }

        absValue /= base;
    }

    // If no character were written
    if(i == 0)
    {
        buffer[i++] = '0';
    }

    // Only base 10 consider negatives value
    if(value < 0 && base == 10)
    {
        buffer[i++] = '-';
    }

    buffer[i] = '\0';

    // Reverse the string here
    strrev(buffer);

    return buffer;
}