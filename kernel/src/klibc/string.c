#include <stddef.h>
#include <stdint.h>

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
    if (dest == src || n == 0)
    {
        return dest;
    }

    uint8_t *pdest = (uint8_t *)dest;
    const uint8_t *psrc = (const uint8_t *)src;

    if (pdest < psrc)
    {
        for (size_t i = 0; i < n; i++)
        {
            pdest[i] = psrc[i];
        }
    }
    else
    {
        for (size_t i = n; i > 0; i--)
        {
            pdest[i - 1] = psrc[i - 1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;

    for (size_t i = 0; i < n; i++)
    {
        if (p1[i] != p2[i])
        {
            return (int)p1[i] - (int)p2[i];
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

size_t strlen(const char *str)
{
    if (str == NULL)
    {
        return 0;
    }

    size_t len = 0;
    while (*str != '\0')
    {
        len++;
        str++;
    }

    return len;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 != '\0' && (*s1 == *s2))
    {
        s1++;
        s2++;
    }

    return (int)((uint8_t)*s1) - (int)((uint8_t)*s2);
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        if (s1[i] != s2[i])
        {
            return (int)((uint8_t)s1[i]) - (int)((uint8_t)s2[i]);
        }

        if (s1[i] == '\0')
        {
            break;
        }
    }
    return 0;
}

char *strcpy(char *dest, const char *src)
{
    char *ret = dest;

    while ((*dest++ = *src++) != '\0')
    {
    }

    return ret;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }

    for (; i < n; i++)
    {
        dest[i] = '\0';
    }

    return dest;
}

char *strcat(char *dest, const char *src)
{
    char *ret = dest;

    while (*dest != '\0')
    {
        dest++;
    }

    while ((*dest++ = *src++) != '\0')
    {
    }

    return ret;
}

// +------------------------+
// | NOT STANDARD FUNCTIONS |
// +------------------------+

char *strrev(char *str)
{
    if (str == NULL || *str == '\0')
    {
        return str;
    }

    size_t len = strlen(str);

    for (size_t i = 0; i < len / 2; i++)
    {
        char tmp = str[i];
        str[i] = str[len - i - 1];
        str[len - i - 1] = tmp;
    }

    return str;
}