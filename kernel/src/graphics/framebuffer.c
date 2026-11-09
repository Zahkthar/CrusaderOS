#include "graphics/framebuffer.h"

#include <stddef.h>
#include <stdint.h>

#include "graphics/font.h"
#include "klibc/math.h"
#include "klibc/string.h"
#include "memory/heap.h"

// +-----------------------------------+
// | DOUBLE BUFFERING (BACKBUFFER)     |
// +-----------------------------------+

static uint32_t *s_backbuffer = NULL;
static size_t s_backbuffer_size = 0;
static bool s_backbuffer_enabled = false;
static int s_dirty_min_y = 0;
static int s_dirty_max_y = -1;

void framebuffer_mark_dirty(int min_y, int max_y)
{
    if (min_y < 0)
    {
        min_y = 0;
    }
    if (max_y < min_y)
    {
        return;
    }

    if (s_dirty_max_y < s_dirty_min_y)
    {
        s_dirty_min_y = min_y;
        s_dirty_max_y = max_y;
    }
    else
    {
        if (min_y < s_dirty_min_y)
        {
            s_dirty_min_y = min_y;
        }
        if (max_y > s_dirty_max_y)
        {
            s_dirty_max_y = max_y;
        }
    }
}

bool framebuffer_init_backbuffer(struct limine_framebuffer *framebuffer)
{
    if (framebuffer == NULL || framebuffer->width == 0 || framebuffer->height == 0)
    {
        s_backbuffer_enabled = false;
        return false;
    }

    size_t pixel_count = framebuffer->width * framebuffer->height;
    size_t required_bytes = pixel_count * sizeof(uint32_t);

    if (s_backbuffer != NULL)
    {
        kfree(s_backbuffer);
        s_backbuffer = NULL;
        s_backbuffer_size = 0;
        s_backbuffer_enabled = false;
    }

    s_backbuffer = (uint32_t *)kmalloc(required_bytes);
    if (s_backbuffer == NULL)
    {
        s_backbuffer_enabled = false;
        return false;
    }

    memset(s_backbuffer, 0, required_bytes);
    s_backbuffer_size = required_bytes;
    s_backbuffer_enabled = true;
    s_dirty_min_y = 0;
    s_dirty_max_y = (int)framebuffer->height;
    return true;
}

bool framebuffer_is_backbuffer_enabled(void)
{
    return s_backbuffer_enabled;
}

uint32_t *framebuffer_get_backbuffer(void)
{
    return s_backbuffer;
}

void framebuffer_swap(struct limine_framebuffer *framebuffer)
{
    if (framebuffer == NULL || framebuffer->address == NULL || !s_backbuffer_enabled)
    {
        return;
    }

    if (s_dirty_max_y < s_dirty_min_y)
    {
        return;
    }

    int start_y = s_dirty_min_y < 0 ? 0 : s_dirty_min_y;
    int end_y = s_dirty_max_y > (int)framebuffer->height ? (int)framebuffer->height : s_dirty_max_y;

    if (start_y >= end_y)
    {
        s_dirty_min_y = (int)framebuffer->height;
        s_dirty_max_y = -1;
        return;
    }

    size_t bytes_per_pixel = (size_t)(framebuffer->bpp / 8);
    if (bytes_per_pixel == 0)
    {
        bytes_per_pixel = 4;
    }

    volatile uint8_t *vram = (volatile uint8_t *)framebuffer->address;

    // Fast path: full buffer blit if all lines are dirty and pitch is packed
    if (start_y == 0 && (size_t)end_y == framebuffer->height && bytes_per_pixel == 4 &&
        framebuffer->pitch == framebuffer->width * 4)
    {
        memcpy(framebuffer->address, s_backbuffer, framebuffer->width * framebuffer->height * 4);
    }
    // Slices of 32 bpp rows with packed pitch
    else if (bytes_per_pixel == 4 && framebuffer->pitch == framebuffer->width * 4)
    {
        size_t offset = (size_t)start_y * framebuffer->width * 4;
        size_t count = (size_t)(end_y - start_y) * framebuffer->width * 4;
        memcpy((void *)(vram + offset), (const void *)((uint8_t *)s_backbuffer + offset), count);
    }
    // 32 bpp row by row (pitch > width * 4)
    else if (bytes_per_pixel == 4)
    {
        for (int y = start_y; y < end_y; y++)
        {
            memcpy((void *)(vram + (size_t)y * framebuffer->pitch), &s_backbuffer[(size_t)y * framebuffer->width],
                   framebuffer->width * 4);
        }
    }
    // 16 bpp: convert each 32-bit backbuffer pixel to 16-bit
    else if (bytes_per_pixel == 2)
    {
        for (int y = start_y; y < end_y; y++)
        {
            volatile uint16_t *dst_row = (volatile uint16_t *)(vram + (size_t)y * framebuffer->pitch);
            const uint32_t *src_row = &s_backbuffer[(size_t)y * framebuffer->width];
            for (size_t x = 0; x < framebuffer->width; x++)
            {
                dst_row[x] = (uint16_t)src_row[x];
            }
        }
    }
    // 24 bpp: write 3 bytes per pixel
    else if (bytes_per_pixel == 3)
    {
        for (int y = start_y; y < end_y; y++)
        {
            volatile uint8_t *dst_row = vram + (size_t)y * framebuffer->pitch;
            const uint32_t *src_row = &s_backbuffer[(size_t)y * framebuffer->width];
            for (size_t x = 0; x < framebuffer->width; x++)
            {
                uint32_t pixel = src_row[x];
                dst_row[x * 3 + 0] = (uint8_t)(pixel & 0xFF);
                dst_row[x * 3 + 1] = (uint8_t)((pixel >> 8) & 0xFF);
                dst_row[x * 3 + 2] = (uint8_t)((pixel >> 16) & 0xFF);
            }
        }
    }

    s_dirty_min_y = (int)framebuffer->height;
    s_dirty_max_y = -1;
}

// +-----------------------------------+
// | COLOR FORMATTING HELPER           |
// +-----------------------------------+

uint32_t framebuffer_format_color(const struct limine_framebuffer *framebuffer, uint32_t rgbColor)
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

static inline void put_pixel_raw(struct limine_framebuffer *framebuffer, int x, int y, uint32_t pixel)
{
    framebuffer_mark_dirty(y, y + 1);
    if (s_backbuffer_enabled)
    {
        s_backbuffer[(size_t)y * framebuffer->width + (size_t)x] = pixel;
    }
    else
    {
        size_t bytes_per_pixel = (size_t)(framebuffer->bpp / 8);
        if (bytes_per_pixel == 0)
        {
            bytes_per_pixel = 4;
        }

        volatile uint8_t *fb = (volatile uint8_t *)framebuffer->address;
        size_t offset = (size_t)y * (size_t)framebuffer->pitch + (size_t)x * bytes_per_pixel;

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
}

// +--------------------------------+
// | COHEN-SUTHERLAND LINE CLIPPING |
// +--------------------------------+

#define CS_INSIDE 0 // 0000
#define CS_LEFT   1 // 0001
#define CS_RIGHT  2 // 0010
#define CS_BOTTOM 4 // 0100
#define CS_TOP    8 // 1000

static int compute_outcode(int x, int y, int xmin, int ymin, int xmax, int ymax)
{
    int code = CS_INSIDE;
    if (x < xmin)
    {
        code |= CS_LEFT;
    }
    else if (x > xmax)
    {
        code |= CS_RIGHT;
    }

    if (y < ymin)
    {
        code |= CS_TOP;
    }
    else if (y > ymax)
    {
        code |= CS_BOTTOM;
    }

    return code;
}

static bool clip_line(int *x0, int *y0, int *x1, int *y1, int xmin, int ymin, int xmax, int ymax)
{
    int code0 = compute_outcode(*x0, *y0, xmin, ymin, xmax, ymax);
    int code1 = compute_outcode(*x1, *y1, xmin, ymin, xmax, ymax);
    bool accept = false;

    while (true)
    {
        if ((code0 | code1) == 0)
        {
            // Both points inside
            accept = true;
            break;
        }
        else if (code0 & code1)
        {
            // Both points share an outside zone -> trivial reject
            break;
        }
        else
        {
            int x = 0, y = 0;
            int code_out = code0 != 0 ? code0 : code1;

            if (code_out & CS_BOTTOM)
            {
                x = *x0 + (int)((int64_t)(*x1 - *x0) * (ymax - *y0) / (*y1 - *y0));
                y = ymax;
            }
            else if (code_out & CS_TOP)
            {
                x = *x0 + (int)((int64_t)(*x1 - *x0) * (ymin - *y0) / (*y1 - *y0));
                y = ymin;
            }
            else if (code_out & CS_RIGHT)
            {
                y = *y0 + (int)((int64_t)(*y1 - *y0) * (xmax - *x0) / (*x1 - *x0));
                x = xmax;
            }
            else if (code_out & CS_LEFT)
            {
                y = *y0 + (int)((int64_t)(*y1 - *y0) * (xmin - *x0) / (*x1 - *x0));
                x = xmin;
            }

            if (code_out == code0)
            {
                *x0 = x;
                *y0 = y;
                code0 = compute_outcode(*x0, *y0, xmin, ymin, xmax, ymax);
            }
            else
            {
                *x1 = x;
                *y1 = y;
                code1 = compute_outcode(*x1, *y1, xmin, ymin, xmax, ymax);
            }
        }
    }

    return accept;
}

// +-----------------------------------+
// | LIMINE FRAMEBUFFER DRAW FUNCTIONS |
// +-----------------------------------+

void framebuffer_clear(struct limine_framebuffer *framebuffer, uint32_t rgbColor)
{
    if (framebuffer == NULL)
    {
        return;
    }
    framebuffer_fillRect(framebuffer, (Rect){0, 0, (int)framebuffer->width, (int)framebuffer->height}, rgbColor);
}

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
    put_pixel_raw(framebuffer, position.x, position.y, pixel);
}

void framebuffer_drawLine(struct limine_framebuffer *framebuffer, Vec2 posA, Vec2 posB, uint32_t rgbColor)
{
    if (framebuffer == NULL || framebuffer->address == NULL)
    {
        return;
    }

    int x0 = posA.x;
    int y0 = posA.y;
    int x1 = posB.x;
    int y1 = posB.y;

    // If line completely off-screen
    if (!clip_line(&x0, &y0, &x1, &y1, 0, 0, (int)framebuffer->width - 1, (int)framebuffer->height - 1))
    {
        return;
    }

    uint32_t pixel = framebuffer_format_color(framebuffer, rgbColor);

    int deltaX = abs(x1 - x0);
    int deltaY = -abs(y1 - y0);

    int signX = x0 < x1 ? 1 : -1;
    int signY = y0 < y1 ? 1 : -1;

    int error = deltaX + deltaY;

    while (1)
    {
        put_pixel_raw(framebuffer, x0, y0, pixel);

        if (x0 == x1 && y0 == y1)
        {
            break;
        }

        int error2 = error * 2;
        if (error2 >= deltaY)
        {
            error += deltaY;
            x0 += signX;
        }
        if (error2 <= deltaX)
        {
            error += deltaX;
            y0 += signY;
        }
    }
}

void framebuffer_drawRect(struct limine_framebuffer *framebuffer, Rect rect, uint32_t rgbColor)
{
    if (framebuffer == NULL || framebuffer->address == NULL || rect.w <= 0 || rect.h <= 0)
    {
        return;
    }

    // Rect is off-screen
    if (rect.x >= (int)framebuffer->width || (rect.x + rect.w) <= 0 || rect.y >= (int)framebuffer->height ||
        (rect.y + rect.h) <= 0)
    {
        return;
    }

    // UP
    framebuffer_drawLine(framebuffer, (Vec2){rect.x, rect.y}, (Vec2){rect.x + rect.w - 1, rect.y}, rgbColor);
    if (rect.h > 1)
    {
        // DOWN
        framebuffer_drawLine(framebuffer, (Vec2){rect.x, rect.y + rect.h - 1},
                             (Vec2){rect.x + rect.w - 1, rect.y + rect.h - 1}, rgbColor);
    }

    if (rect.h > 2)
    {
        // LEFT
        framebuffer_drawLine(framebuffer, (Vec2){rect.x, rect.y + 1}, (Vec2){rect.x, rect.y + rect.h - 2}, rgbColor);
        if (rect.w > 1)
        {
            // RIGHT
            framebuffer_drawLine(framebuffer, (Vec2){rect.x + rect.w - 1, rect.y + 1},
                                 (Vec2){rect.x + rect.w - 1, rect.y + rect.h - 2}, rgbColor);
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
    framebuffer_mark_dirty(y1, y2);

    if (s_backbuffer_enabled)
    {
        for (int y = y1; y < y2; y++)
        {
            uint32_t *row = &s_backbuffer[(size_t)y * framebuffer->width];
            for (int x = x1; x < x2; x++)
            {
                row[x] = pixel;
            }
        }
    }
    else
    {
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
                    put_pixel_raw(framebuffer, x, y, pixel);
                }
            }
        }
    }
}

void framebuffer_scroll_up(struct limine_framebuffer *framebuffer, int pixels, uint32_t fill_color)
{
    if (framebuffer == NULL || pixels <= 0 || (uint64_t)pixels >= framebuffer->height)
    {
        return;
    }

    uint32_t pixel = framebuffer_format_color(framebuffer, fill_color);
    framebuffer_mark_dirty(0, (int)framebuffer->height);

    if (s_backbuffer_enabled && s_backbuffer != NULL)
    {
        size_t total_pixels = framebuffer->width * framebuffer->height;
        size_t move_pixels = framebuffer->width * (framebuffer->height - (size_t)pixels);
        size_t offset_pixels = framebuffer->width * (size_t)pixels;

        memmove(s_backbuffer, s_backbuffer + offset_pixels, move_pixels * sizeof(uint32_t));

        for (size_t i = move_pixels; i < total_pixels; i++)
        {
            s_backbuffer[i] = pixel;
        }
    }
    else
    {
        size_t bytes_per_pixel = (size_t)(framebuffer->bpp / 8);
        if (bytes_per_pixel == 0)
        {
            bytes_per_pixel = 4;
        }

        volatile uint8_t *fb = (volatile uint8_t *)framebuffer->address;
        size_t move_bytes = (framebuffer->height - (size_t)pixels) * framebuffer->pitch;
        size_t offset_bytes = (size_t)pixels * framebuffer->pitch;

        memmove((void *)fb, (const void *)(fb + offset_bytes), move_bytes);

        for (size_t y = framebuffer->height - (size_t)pixels; y < framebuffer->height; y++)
        {
            volatile uint8_t *row = fb + y * framebuffer->pitch;
            for (size_t x = 0; x < framebuffer->width; x++)
            {
                if (bytes_per_pixel == 4)
                {
                    *(volatile uint32_t *)(row + x * 4) = pixel;
                }
                else
                {
                    put_pixel_raw(framebuffer, (int)x, (int)y, pixel);
                }
            }
        }
    }
}

void framebuffer_drawChar(struct limine_framebuffer *framebuffer, int x, int y, char c, uint32_t fg, uint32_t bg)
{
    font_draw_char(framebuffer, x, y, c, fg, bg);
}

void framebuffer_drawString(struct limine_framebuffer *framebuffer, int x, int y, const char *str, uint32_t fg,
                            uint32_t bg)
{
    font_draw_string(framebuffer, x, y, str, fg, bg);
}