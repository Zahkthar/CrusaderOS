#include "klibc/stdlib.h"

#include "klibc/math.h"
#include "klibc/string.h"

#include <stdbool.h>

// +-----------------------------+
// | [NUMERIC <=> STR] FUNCTIONS |
// +-----------------------------+

char *itoa(int value, char *buffer, int base)
{
    if (buffer == NULL)
    {
        return NULL;
    }

    if (base < 2 || base > 36)
    {
        buffer[0] = '\0';
        return buffer;
    }

    bool is_negative = false;
    unsigned int abs_value;

    // Only base 10 considers negative values
    if (value < 0 && base == 10)
    {
        is_negative = true;
        abs_value = 0 - (unsigned int)value;
    }
    else
    {
        abs_value = (unsigned int)value;
    }

    int i = 0;

    if (abs_value == 0)
    {
        buffer[i++] = '0';
    }
    else
    {
        while (abs_value != 0)
        {
            unsigned int r = abs_value % (unsigned int)base;

            if (r >= 10)
            {
                buffer[i++] = (char)('A' + (r - 10));
            }
            else
            {
                buffer[i++] = (char)('0' + r);
            }

            abs_value /= (unsigned int)base;
        }
    }

    if (is_negative)
    {
        buffer[i++] = '-';
    }

    buffer[i] = '\0';

    strrev(buffer);

    return buffer;
}