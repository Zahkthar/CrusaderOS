#include "graphics/framebuffer.h"

#include <stdint.h>

#include "klibc/math.h"

// +-----------------------------------+
// | LIMINE FRAMEBUFFER DRAW FUNCTIONS |
// +-----------------------------------+

void framebuffer_putPixel(struct limine_framebuffer *framebuffer, Vec2 position, int rgbColor)
{
    ((uint32_t *)framebuffer->address)[position.y * (framebuffer->pitch / 4) + position.x] = rgbColor;
}

void framebuffer_drawLine(struct limine_framebuffer *framebuffer, Vec2 posA, Vec2 posB, int rgbColor)
{
    int deltaX = abs(posB.x - posA.x);
    int deltaY = -abs(posB.y - posA.y);

    int signX = posA.x < posB.x ? 1 : -1; // isALeft
    int signY = posA.y < posB.y ? 1 : -1; // isAUp

    int error = deltaX + deltaY;

    while (posA.x != posB.x || posA.y != posB.y)
    {
        framebuffer_putPixel(framebuffer, (Vec2){posA.x, posA.y}, rgbColor);

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

void framebuffer_drawRect(struct limine_framebuffer *framebuffer, Rect rect, int rgbColor)
{
    for (int i = rect.x; i <= rect.x + rect.w; i++) // Up and down
    {
        framebuffer_putPixel(framebuffer, (Vec2){i, rect.y}, rgbColor);
        framebuffer_putPixel(framebuffer, (Vec2){i, rect.y + rect.h}, rgbColor);
    }

    for (int i = rect.y; i <= rect.y + rect.h; i++) // Left and right
    {
        framebuffer_putPixel(framebuffer, (Vec2){rect.x, i}, rgbColor);
        framebuffer_putPixel(framebuffer, (Vec2){rect.x + rect.w, i}, rgbColor);
    }
}

void framebuffer_fillRect(struct limine_framebuffer *framebuffer, Rect rect, int rgbColor)
{
    for (int i = rect.x; i < rect.x + rect.w; i++)
    {
        for (int j = rect.y; j < rect.y + rect.h; j++)
        {
            framebuffer_putPixel(framebuffer, (Vec2){i, j}, rgbColor);
        }
    }
}