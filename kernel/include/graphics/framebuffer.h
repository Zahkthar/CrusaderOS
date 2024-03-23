#ifndef FRAMEBUFFER_H_INCLUDED
#define FRAMEBUFFER_H_INCLUDED

#include <stdint.h>

#include <limine.h>

#include "maths/defs.h"

// +-----------------------------------+
// | LIMINE FRAMEBUFFER DRAW FUNCTIONS |
// +-----------------------------------+

void framebuffer_putPixel(struct limine_framebuffer *framebuffer, Vec2 position, int rgbColor);

void framebuffer_drawLine(struct limine_framebuffer *framebuffer, Vec2 posA, Vec2 posB, int rgbColor);

void framebuffer_drawRect(struct limine_framebuffer *framebuffer, Rect rect, int rgbColor);
void framebuffer_fillRect(struct limine_framebuffer *framebuffer, Rect rect, int rgbColor);

#endif // FRAMEBUFFER_H_INCLUDED