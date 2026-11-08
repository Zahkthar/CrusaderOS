#include "klibc/stdio.h"

#include <stdbool.h>

#include "drivers/serial.h"
#include "klibc/string.h"

static void format_unsigned(char **buf, size_t *remaining, size_t *written, uint64_t val, int base, bool uppercase,
                            int width, bool pad_zero)
{
    if (buf == NULL || remaining == NULL || written == NULL || base < 2 || base > 16)
    {
        return;
    }

    char tmp[65];
    int pos = 0;
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";

    if (val == 0)
    {
        tmp[pos++] = '0';
    }
    else
    {
        while (val != 0)
        {
            tmp[pos++] = digits[val % (uint64_t)base];
            val /= (uint64_t)base;
        }
    }

    int pad = (width > pos) ? (width - pos) : 0;
    char pad_char = pad_zero ? '0' : ' ';

    while (pad-- > 0)
    {
        if (*remaining > 1)
        {
            *(*buf)++ = pad_char;
            (*remaining)--;
        }
        (*written)++;
    }

    while (pos > 0)
    {
        char c = tmp[--pos];
        if (*remaining > 1)
        {
            *(*buf)++ = c;
            (*remaining)--;
        }
        (*written)++;
    }
}

static void format_signed(char **buf, size_t *remaining, size_t *written, int64_t val, int width, bool pad_zero)
{
    bool is_negative = false;
    uint64_t uval;

    if (val < 0)
    {
        is_negative = true;
        uval = (uint64_t)(0 - (uint64_t)val);
    }
    else
    {
        uval = (uint64_t)val;
    }

    if (is_negative && pad_zero)
    {
        if (*remaining > 1)
        {
            *(*buf)++ = '-';
            (*remaining)--;
        }
        (*written)++;
        if (width > 0)
        {
            width--;
        }
        is_negative = false;
    }

    char tmp[65];
    int pos = 0;

    if (uval == 0)
    {
        tmp[pos++] = '0';
    }
    else
    {
        while (uval != 0)
        {
            tmp[pos++] = (char)('0' + (uval % 10));
            uval /= 10;
        }
    }

    if (is_negative)
    {
        tmp[pos++] = '-';
    }

    int pad = (width > pos) ? (width - pos) : 0;
    char pad_char = pad_zero ? '0' : ' ';

    while (pad-- > 0)
    {
        if (*remaining > 1)
        {
            *(*buf)++ = pad_char;
            (*remaining)--;
        }
        (*written)++;
    }

    while (pos > 0)
    {
        char c = tmp[--pos];
        if (*remaining > 1)
        {
            *(*buf)++ = c;
            (*remaining)--;
        }
        (*written)++;
    }
}

int vsnprintf(char *buffer, size_t size, const char *format, va_list args)
{
    if (format == NULL)
    {
        return -1;
    }

    char *buf = buffer;
    size_t remaining = (buffer != NULL) ? size : 0;
    size_t written = 0;

    while (*format != '\0')
    {
        if (*format != '%')
        {
            if (remaining > 1)
            {
                *buf++ = *format;
                remaining--;
            }
            written++;
            format++;
            continue;
        }

        format++; // Skip '%'

        // Width and padding flags
        bool pad_zero = false;
        int width = 0;

        if (*format == '0')
        {
            pad_zero = true;
            format++;
        }

        while (*format >= '0' && *format <= '9')
        {
            width = (width * 10) + (*format - '0');
            format++;
        }

        // Length modifiers
        bool is_long = false;
        bool is_long_long = false;

        if (*format == 'l')
        {
            is_long = true;
            format++;
            if (*format == 'l')
            {
                is_long_long = true;
                format++;
            }
        }

        switch (*format)
        {
        case 'c':
        {
            char c = (char)va_arg(args, int);
            if (remaining > 1)
            {
                *buf++ = c;
                remaining--;
            }
            written++;
            break;
        }
        case 's':
        {
            const char *str = va_arg(args, const char *);
            if (str == NULL)
            {
                str = "(null)";
            }

            size_t len = strlen(str);
            int pad = (width > (int)len) ? (width - (int)len) : 0;

            while (pad-- > 0)
            {
                if (remaining > 1)
                {
                    *buf++ = ' ';
                    remaining--;
                }
                written++;
            }

            while (*str != '\0')
            {
                if (remaining > 1)
                {
                    *buf++ = *str;
                    remaining--;
                }
                written++;
                str++;
            }
            break;
        }
        case 'd':
        case 'i':
        {
            int64_t val;
            if (is_long_long || is_long)
            {
                val = va_arg(args, int64_t);
            }
            else
            {
                val = (int64_t)va_arg(args, int);
            }
            format_signed(&buf, &remaining, &written, val, width, pad_zero);
            break;
        }
        case 'u':
        {
            uint64_t val;
            if (is_long_long || is_long)
            {
                val = va_arg(args, uint64_t);
            }
            else
            {
                val = (uint64_t)va_arg(args, unsigned int);
            }
            format_unsigned(&buf, &remaining, &written, val, 10, false, width, pad_zero);
            break;
        }
        case 'x':
        case 'X':
        {
            bool uppercase = (*format == 'X');
            uint64_t val;
            if (is_long_long || is_long)
            {
                val = va_arg(args, uint64_t);
            }
            else
            {
                val = (uint64_t)va_arg(args, unsigned int);
            }
            format_unsigned(&buf, &remaining, &written, val, 16, uppercase, width, pad_zero);
            break;
        }
        case 'p':
        {
            uintptr_t val = (uintptr_t)va_arg(args, void *);
            // Add "0x" prefix
            if (remaining > 1)
            {
                *buf++ = '0';
                remaining--;
            }
            written++;
            if (remaining > 1)
            {
                *buf++ = 'x';
                remaining--;
            }
            written++;

            int hex_width = (width > 2) ? (width - 2) : 16;
            format_unsigned(&buf, &remaining, &written, (uint64_t)val, 16, false, hex_width, true);
            break;
        }
        case '%':
        {
            if (remaining > 1)
            {
                *buf++ = '%';
                remaining--;
            }
            written++;
            break;
        }
        default:
            // Unknown specifier, emit as-is
            if (remaining > 1)
            {
                *buf++ = '%';
                remaining--;
            }
            written++;
            if (*format != '\0')
            {
                if (remaining > 1)
                {
                    *buf++ = *format;
                    remaining--;
                }
                written++;
            }
            break;
        }

        if (*format != '\0')
        {
            format++;
        }
    }

    if (buffer != NULL && size > 0)
    {
        *buf = '\0';
    }

    return (int)written;
}

int snprintf(char *buffer, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, size, format, args);
    va_end(args);
    return ret;
}

#define SPRINTF_MAX_SAFE_SIZE 4096

int sprintf(char *buffer, const char *format, ...)
{
    if (buffer == NULL || format == NULL)
    {
        return -1;
    }

    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, SPRINTF_MAX_SAFE_SIZE, format, args);
    va_end(args);
    return ret;
}

void kprintf(const char *format, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    serial_write(SERIAL_COM1, buffer);
}
