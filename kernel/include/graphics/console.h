#ifndef GRAPHICS_CONSOLE_H_INCLUDED
#define GRAPHICS_CONSOLE_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <limine.h>

// +-----------+
// | FUNCTIONS |
// +-----------+

void console_init(struct limine_framebuffer *framebuffer);
bool console_is_initialized(void);

void console_clear(void);
void console_putchar(char c);
void console_write(const char *data, size_t length);

void console_set_color(uint32_t fg, uint32_t bg);
void console_get_color(uint32_t *out_fg, uint32_t *out_bg);

void console_set_cursor(int col, int row);
void console_get_cursor(int *out_col, int *out_row);

int console_get_cols(void);
int console_get_rows(void);

void console_flush(void);

#endif // GRAPHICS_CONSOLE_H_INCLUDED
