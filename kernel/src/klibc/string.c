#include <stdint.h>
#include <stddef.h>

#include "klibc/string.h"

void *memcpy(void *dest, const void *src, size_t n)
{
    uint8_t *pdest = dest;
    const uint8_t *psrc = src;

    for (size_t i = 0; i < n; i++)
    {
        pdest[i] = psrc[i];
    }

    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    uint8_t *pdest = dest;
    const uint8_t *psrc = src;

    if (src > dest)
    {
        for (size_t i = 0; i < n; i++)
        {
            pdest[i] = psrc[i];
        }
    }
    else if (src < dest)
    {
        for (size_t i = n - 1; i > 0; i--)
        {
            pdest[i] = psrc[i];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *p1 = s1;
    const uint8_t *p2 = s2;

    for (size_t i = 0; i < n; i++)
    {
        if (p1[i] != p2[i])
        {
            return p1[i] < p2[i] ? -1 : 1;
        }
    }

    return 0;
}

void *memset(void *ptr, int value, size_t count)
{
    uint8_t *p = ptr;

    for (size_t i = 0; i < count; i++)
    {
        p[i] = (uint8_t)value;
    }

    return ptr;
}