#include "graphics/framebuffer.h"

#include <stddef.h>
#include <stdint.h>

#include "klibc/math.h"

// +-----------------------------------+
// | COLOR FORMATTING HELPER           |
// +-----------------------------------+

static uint32_t framebuffer_format_color(const struct limine_framebuffer *framebuffer, uint32_t rgbColor)
{
    // If mask sizes are not provided by the bootloader, fall back to direct 24/32-bit RGB
    if (framebuffer->red_mask_size == 0 && framebuffer->green_mask_size == 0 && framebuffer->blue_mask_size == 0)
    {
        return rgbColor;
    }

    uint32_t r = (rgbColor >> 16) & 0xFF;
    uint32_t g = (rgbColor >> 8) & 0xFF;
    uint32_t b = rgbColor & 0xFF;

    uint32_t pixel = 0;

    pixel |= (framebuffer->red_mask_size >= 8)
                 ? ((r << (framebuffer->red_mask_size - 8)) << framebuffer->red_mask_shift)
                 : ((r >> (8 - framebuffer->red_mask_size)) << framebuffer->red_mask_shift);
    pixel |= (framebuffer->green_mask_size >= 8)
                 ? ((g << (framebuffer->green_mask_size - 8)) << framebuffer->green_mask_shift)
                 : ((g >> (8 - framebuffer->green_mask_size)) << framebuffer->green_mask_shift);
    pixel |= (framebuffer->blue_mask_size >= 8)
                 ? ((b << (framebuffer->blue_mask_size - 8)) << framebuffer->blue_mask_shift)
                 : ((b >> (8 - framebuffer->blue_mask_size)) << framebuffer->blue_mask_shift);

    return pixel;
}

// +-----------------------------------+
// | LIMINE FRAMEBUFFER DRAW FUNCTIONS |
// +-----------------------------------+

void framebuffer_putPixel(struct limine_framebuffer *framebuffer, Vec2 position, uint32_t rgbColor)
{
    if (framebuffer == NULL || framebuffer->address == NULL)
    {
        return;
    }

    if (position.x < 0 || (uint64_t)position.x >= framebuffer->width || position.y < 0 ||
        (uint64_t)position.y >= framebuffer->height)
    {
        return;
    }

    uint32_t pixel = framebuffer_format_color(framebuffer, rgbColor);
    size_t bytes_per_pixel = (size_t)(framebuffer->bpp / 8);
    if (bytes_per_pixel == 0)
    {
        bytes_per_pixel = 4;
    }

    volatile uint8_t *fb = (volatile uint8_t *)framebuffer->address;
    size_t offset = (size_t)position.y * (size_t)framebuffer->pitch + (size_t)position.x * bytes_per_pixel;

    if (bytes_per_pixel == 4)
    {
        *(volatile uint32_t *)(fb + offset) = pixel;
    }
    else if (bytes_per_pixel == 2)
    {
        *(volatile uint16_t *)(fb + offset) = (uint16_t)pixel;
    }
    else if (bytes_per_pixel == 3)
    {
        fb[offset + 0] = (uint8_t)(pixel & 0xFF);
        fb[offset + 1] = (uint8_t)((pixel >> 8) & 0xFF);
        fb[offset + 2] = (uint8_t)((pixel >> 16) & 0xFF);
    }
}

void framebuffer_drawLine(struct limine_framebuffer *framebuffer, Vec2 posA, Vec2 posB, uint32_t rgbColor)
{
    int deltaX = abs(posB.x - posA.x);
    int deltaY = -abs(posB.y - posA.y);

    int signX = posA.x < posB.x ? 1 : -1;
    int signY = posA.y < posB.y ? 1 : -1;

    int error = deltaX + deltaY;

    while (1)
    {
        framebuffer_putPixel(framebuffer, (Vec2){posA.x, posA.y}, rgbColor);

        if (posA.x == posB.x && posA.y == posB.y)
        {
            break;
        }

        int error2 = error * 2;
        if (error2 >= deltaY)
        {
            error += deltaY;
            posA.x += signX;
        }
        if (error2 <= deltaX)
        {
            error += deltaX;
            posA.y += signY;
        }
    }
}

void framebuffer_drawRect(struct limine_framebuffer *framebuffer, Rect rect, uint32_t rgbColor)
{
    if (rect.w <= 0 || rect.h <= 0)
    {
        return;
    }

    for (int i = rect.x; i < rect.x + rect.w; i++)
    {
        framebuffer_putPixel(framebuffer, (Vec2){i, rect.y}, rgbColor);
        if (rect.h > 1)
        {
            framebuffer_putPixel(framebuffer, (Vec2){i, rect.y + rect.h - 1}, rgbColor);
        }
    }

    for (int j = rect.y + 1; j < rect.y + rect.h - 1; j++)
    {
        framebuffer_putPixel(framebuffer, (Vec2){rect.x, j}, rgbColor);
        if (rect.w > 1)
        {
            framebuffer_putPixel(framebuffer, (Vec2){rect.x + rect.w - 1, j}, rgbColor);
        }
    }
}

void framebuffer_fillRect(struct limine_framebuffer *framebuffer, Rect rect, uint32_t rgbColor)
{
    if (framebuffer == NULL || framebuffer->address == NULL || rect.w <= 0 || rect.h <= 0)
    {
        return;
    }

    int x1 = rect.x < 0 ? 0 : rect.x;
    int y1 = rect.y < 0 ? 0 : rect.y;
    int x2 = (rect.x + rect.w) > (int)framebuffer->width ? (int)framebuffer->width : (rect.x + rect.w);
    int y2 = (rect.y + rect.h) > (int)framebuffer->height ? (int)framebuffer->height : (rect.y + rect.h);

    if (x1 >= x2 || y1 >= y2)
    {
        return;
    }

    uint32_t pixel = framebuffer_format_color(framebuffer, rgbColor);
    size_t bytes_per_pixel = (size_t)(framebuffer->bpp / 8);
    volatile uint8_t *fb = (volatile uint8_t *)framebuffer->address;

    if (bytes_per_pixel == 4 || bytes_per_pixel == 0)
    {
        for (int y = y1; y < y2; y++)
        {
            volatile uint32_t *row = (volatile uint32_t *)(fb + (size_t)y * framebuffer->pitch);
            for (int x = x1; x < x2; x++)
            {
                row[x] = pixel;
            }
        }
    }
    else
    {
        for (int y = y1; y < y2; y++)
        {
            for (int x = x1; x < x2; x++)
            {
                framebuffer_putPixel(framebuffer, (Vec2){x, y}, rgbColor);
            }
        }
    }
}