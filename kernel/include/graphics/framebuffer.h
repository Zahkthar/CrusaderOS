#ifndef FRAMEBUFFER_H_INCLUDED
#define FRAMEBUFFER_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include <limine.h>

#include "maths/defs.h"

// +-----------------------------------+
// | LIMINE FRAMEBUFFER DRAW FUNCTIONS |
// +-----------------------------------+

bool framebuffer_init_backbuffer(struct limine_framebuffer *framebuffer);
void framebuffer_swap(struct limine_framebuffer *framebuffer);
void framebuffer_clear(struct limine_framebuffer *framebuffer, uint32_t rgbColor);

void framebuffer_putPixel(struct limine_framebuffer *framebuffer, Vec2 position, uint32_t rgbColor);
void framebuffer_drawLine(struct limine_framebuffer *framebuffer, Vec2 posA, Vec2 posB, uint32_t rgbColor);
void framebuffer_drawRect(struct limine_framebuffer *framebuffer, Rect rect, uint32_t rgbColor);
void framebuffer_fillRect(struct limine_framebuffer *framebuffer, Rect rect, uint32_t rgbColor);

#endif // FRAMEBUFFER_H_INCLUDED