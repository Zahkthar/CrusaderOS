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
bool framebuffer_is_backbuffer_enabled(void);
uint32_t *framebuffer_get_backbuffer(void);
void framebuffer_mark_dirty(int min_y, int max_y);
void framebuffer_swap(struct limine_framebuffer *framebuffer);
void framebuffer_clear(struct limine_framebuffer *framebuffer, uint32_t rgbColor);

uint32_t framebuffer_format_color(const struct limine_framebuffer *framebuffer, uint32_t rgbColor);
void framebuffer_scroll_up(struct limine_framebuffer *framebuffer, int pixels, uint32_t fill_color);

void framebuffer_putPixel(struct limine_framebuffer *framebuffer, Vec2 position, uint32_t rgbColor);
void framebuffer_drawLine(struct limine_framebuffer *framebuffer, Vec2 posA, Vec2 posB, uint32_t rgbColor);
void framebuffer_drawRect(struct limine_framebuffer *framebuffer, Rect rect, uint32_t rgbColor);
void framebuffer_fillRect(struct limine_framebuffer *framebuffer, Rect rect, uint32_t rgbColor);

void framebuffer_drawChar(struct limine_framebuffer *framebuffer, int x, int y, char c, uint32_t fg, uint32_t bg);
void framebuffer_drawString(struct limine_framebuffer *framebuffer, int x, int y, const char *str, uint32_t fg,
                            uint32_t bg);

#endif // FRAMEBUFFER_H_INCLUDED