#ifndef GRAPHICS_FONT_H_INCLUDED
#define GRAPHICS_FONT_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <limine.h>

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

#define FONT_WIDTH  8
#define FONT_HEIGHT 16
#define FONT_GLYPHS 256

// PSF1 format structures
#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

typedef struct
{
    uint8_t magic[2];
    uint8_t mode;
    uint8_t charsize;
} __attribute__((packed)) PSF1Header;

// PSF2 format structures
#define PSF2_MAGIC0 0x72
#define PSF2_MAGIC1 0xb5
#define PSF2_MAGIC2 0x4a
#define PSF2_MAGIC3 0x86

typedef struct
{
    uint8_t magic[4];
    uint32_t version;
    uint32_t headersize;
    uint32_t flags;
    uint32_t length;
    uint32_t charsize;
    uint32_t height;
    uint32_t width;
} __attribute__((packed)) PSF2Header;

// +-----------+
// | FUNCTIONS |
// +-----------+

const uint8_t *font_get_glyph(uint8_t c);

void font_draw_char(struct limine_framebuffer *framebuffer, int x, int y, char c, uint32_t fg, uint32_t bg);

void font_draw_string(struct limine_framebuffer *framebuffer, int x, int y, const char *str, uint32_t fg,
                      uint32_t bg);

#endif // GRAPHICS_FONT_H_INCLUDED
