#include "graphics/framebuffer.h"

#include <stddef.h>
#include <stdint.h>

#include "klibc/math.h"

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

    ((uint32_t *)framebuffer->address)[position.y * (framebuffer->pitch / 4) + position.x] = rgbColor;
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
    if (rect.w <= 0 || rect.h <= 0)
    {
        return;
    }

    for (int j = rect.y; j < rect.y + rect.h; j++)
    {
        for (int i = rect.x; i < rect.x + rect.w; i++)
        {
            framebuffer_putPixel(framebuffer, (Vec2){i, j}, rgbColor);
        }
    }
}