#include "graphics/console.h"

#include "graphics/colors.h"
#include "graphics/font.h"
#include "graphics/framebuffer.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "sys/spinlock.h"

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

#define TAB_WIDTH 4

// ANSI escape state machine
#define ANSI_STATE_NORMAL 0
#define ANSI_STATE_ESC    1
#define ANSI_STATE_CSI    2

// +--------------------+
// | INTERNAL VARIABLES |
// +--------------------+

static struct limine_framebuffer *s_framebuffer = NULL;
static int s_cursor_x = 0;
static int s_cursor_y = 0;
static int s_cols = 0;
static int s_rows = 0;

static uint32_t s_fg_color = RGB_WHITE;
static uint32_t s_bg_color = RGB_BLACK;
static bool s_bold = false;

static bool s_console_initialized = false;
static Spinlock s_console_lock = SPINLOCK_INIT;

static int s_ansi_state = ANSI_STATE_NORMAL;
static char s_ansi_buf[32];
static size_t s_ansi_len = 0;

// Standard ANSI 8-color palette
static const uint32_t s_ansi_standard_colors[8] = {
    0x000000, // 0: Black
    0xCC0000, // 1: Red
    0x00CC00, // 2: Green
    0xCCCC00, // 3: Yellow
    0x0066CC, // 4: Blue
    0xCC00CC, // 5: Magenta
    0x00CCCC, // 6: Cyan
    0xCCCCCC  // 7: White
};

// Bright ANSI 8-color palette
static const uint32_t s_ansi_bright_colors[8] = {
    0x555555, // 0: Bright Black (Dark Grey)
    0xFF5555, // 1: Bright Red
    0x55FF55, // 2: Bright Green
    0xFFFF55, // 3: Bright Yellow
    0x5599FF, // 4: Bright Blue
    0xFF55FF, // 5: Bright Magenta
    0x55FFFF, // 6: Bright Cyan
    0xFFFFFF  // 7: Bright White
};

// +------------------+
// | HELPER FUNCTIONS |
// +------------------+

static void console_scroll_unlocked(void)
{
    framebuffer_scroll_up(s_framebuffer, FONT_HEIGHT, s_bg_color);
}

static void console_clear_unlocked(void)
{
    framebuffer_clear(s_framebuffer, s_bg_color);
    s_cursor_x = 0;
    s_cursor_y = 0;
}

static void console_handle_ansi_sgr(const char *buf)
{
    // Parse semicolon-separated integer parameters
    const char *p = buf;
    while (*p != '\0')
    {
        int val = atoi(p);
        if (val == 0)
        {
            s_fg_color = RGB_WHITE;
            s_bg_color = RGB_BLACK;
            s_bold = false;
        }
        else if (val == 1)
        {
            s_bold = true;
        }
        else if (val >= 30 && val <= 37)
        {
            int idx = val - 30;
            s_fg_color = s_bold ? s_ansi_bright_colors[idx] : s_ansi_standard_colors[idx];
        }
        else if (val == 39)
        {
            s_fg_color = RGB_WHITE;
        }
        else if (val >= 40 && val <= 47)
        {
            int idx = val - 40;
            s_bg_color = s_ansi_standard_colors[idx];
        }
        else if (val == 49)
        {
            s_bg_color = RGB_BLACK;
        }
        else if (val >= 90 && val <= 97)
        {
            int idx = val - 90;
            s_fg_color = s_ansi_bright_colors[idx];
        }
        else if (val >= 100 && val <= 107)
        {
            int idx = val - 100;
            s_bg_color = s_ansi_bright_colors[idx];
        }

        while (*p != '\0' && *p != ';')
        {
            p++;
        }
        if (*p == ';')
        {
            p++;
        }
    }
}

static void console_putchar_unlocked(char c)
{
    // ANSI Escape Sequence state machine
    if (s_ansi_state == ANSI_STATE_NORMAL)
    {
        if (c == 0x1B) // ESC
        {
            s_ansi_state = ANSI_STATE_ESC;
            return;
        }
    }
    else if (s_ansi_state == ANSI_STATE_ESC)
    {
        if (c == '[') // CSI: Control Sequence Introducer
        {
            s_ansi_state = ANSI_STATE_CSI;
            s_ansi_len = 0;
            s_ansi_buf[0] = '\0';
            return;
        }
        s_ansi_state = ANSI_STATE_NORMAL;
    }
    else if (s_ansi_state == ANSI_STATE_CSI)
    {
        if ((c >= '0' && c <= '9') || c == ';')
        {
            if (s_ansi_len + 1 < sizeof(s_ansi_buf))
            {
                s_ansi_buf[s_ansi_len++] = c;
                s_ansi_buf[s_ansi_len] = '\0';
            }
            return;
        }
        else if (c == 'm') // SGR: Select Graphic Rendition (Colors)
        {
            console_handle_ansi_sgr(s_ansi_buf);
            s_ansi_state = ANSI_STATE_NORMAL;
            return;
        }
        else if (c == 'J') // Clear display
        {
            console_clear_unlocked();
            s_ansi_state = ANSI_STATE_NORMAL;
            return;
        }
        else
        {
            // Unhandled CSI command, revert to normal
            s_ansi_state = ANSI_STATE_NORMAL;
            return;
        }
    }

    // Standard character handling
    switch (c)
    {
        case '\r':
            s_cursor_x = 0;
            break;

        case '\n':
            s_cursor_x = 0;
            s_cursor_y++;
            if (s_cursor_y >= s_rows)
            {
                console_scroll_unlocked();
                s_cursor_y = s_rows - 1;
            }
            break;

        case '\t':
        {
            int next_tab = (s_cursor_x + TAB_WIDTH) & ~(TAB_WIDTH - 1);
            while (s_cursor_x < next_tab && s_cursor_x < s_cols)
            {
                font_draw_char(s_framebuffer, s_cursor_x * FONT_WIDTH, s_cursor_y * FONT_HEIGHT, ' ', s_fg_color,
                               s_bg_color);
                s_cursor_x++;
            }
            if (s_cursor_x >= s_cols)
            {
                s_cursor_x = 0;
                s_cursor_y++;
                if (s_cursor_y >= s_rows)
                {
                    console_scroll_unlocked();
                    s_cursor_y = s_rows - 1;
                }
            }
            break;
        }

        case '\b':
            if (s_cursor_x > 0)
            {
                s_cursor_x--;
                font_draw_char(s_framebuffer, s_cursor_x * FONT_WIDTH, s_cursor_y * FONT_HEIGHT, ' ', s_fg_color,
                               s_bg_color);
            }
            break;

        default:
            if ((unsigned char)c >= 32)
            {
                font_draw_char(s_framebuffer, s_cursor_x * FONT_WIDTH, s_cursor_y * FONT_HEIGHT, c, s_fg_color,
                               s_bg_color);
                s_cursor_x++;
                if (s_cursor_x >= s_cols)
                {
                    s_cursor_x = 0;
                    s_cursor_y++;
                    if (s_cursor_y >= s_rows)
                    {
                        console_scroll_unlocked();
                        s_cursor_y = s_rows - 1;
                    }
                }
            }
            break;
    }
}

// +-----------+
// | FUNCTIONS |
// +-----------+

void console_init(struct limine_framebuffer *framebuffer)
{
    if (framebuffer == NULL || framebuffer->width == 0 || framebuffer->height == 0)
    {
        return;
    }

    uint64_t flags = spinlock_acquire_irqsave(&s_console_lock);

    s_framebuffer = framebuffer;
    s_cols = (int)(framebuffer->width / FONT_WIDTH);
    s_rows = (int)(framebuffer->height / FONT_HEIGHT);
    s_cursor_x = 0;
    s_cursor_y = 0;
    s_fg_color = RGB_WHITE;
    s_bg_color = RGB_BLACK;
    s_bold = false;
    s_ansi_state = ANSI_STATE_NORMAL;
    s_ansi_len = 0;
    s_console_initialized = true;

    console_clear_unlocked();
    framebuffer_swap(s_framebuffer);

    spinlock_release_irqrestore(&s_console_lock, flags);
}

bool console_is_initialized(void)
{
    return s_console_initialized;
}

void console_clear(void)
{
    if (!s_console_initialized)
    {
        return;
    }

    uint64_t flags = spinlock_acquire_irqsave(&s_console_lock);
    console_clear_unlocked();
    framebuffer_swap(s_framebuffer);
    spinlock_release_irqrestore(&s_console_lock, flags);
}

void console_putchar(char c)
{
    if (!s_console_initialized)
    {
        return;
    }

    uint64_t flags = spinlock_acquire_irqsave(&s_console_lock);
    console_putchar_unlocked(c);
    framebuffer_swap(s_framebuffer);
    spinlock_release_irqrestore(&s_console_lock, flags);
}

void console_write(const char *data, size_t length)
{
    if (!s_console_initialized || data == NULL || length == 0)
    {
        return;
    }

    uint64_t flags = spinlock_acquire_irqsave(&s_console_lock);

    for (size_t i = 0; i < length; i++)
    {
        console_putchar_unlocked(data[i]);
    }

    framebuffer_swap(s_framebuffer);
    spinlock_release_irqrestore(&s_console_lock, flags);
}

void console_set_color(uint32_t fg, uint32_t bg)
{
    uint64_t flags = spinlock_acquire_irqsave(&s_console_lock);
    s_fg_color = fg;
    s_bg_color = bg;
    spinlock_release_irqrestore(&s_console_lock, flags);
}

void console_get_color(uint32_t *out_fg, uint32_t *out_bg)
{
    uint64_t flags = spinlock_acquire_irqsave(&s_console_lock);
    if (out_fg != NULL)
    {
        *out_fg = s_fg_color;
    }
    if (out_bg != NULL)
    {
        *out_bg = s_bg_color;
    }
    spinlock_release_irqrestore(&s_console_lock, flags);
}

void console_set_cursor(int col, int row)
{
    uint64_t flags = spinlock_acquire_irqsave(&s_console_lock);
    if (col >= 0 && col < s_cols)
    {
        s_cursor_x = col;
    }
    if (row >= 0 && row < s_rows)
    {
        s_cursor_y = row;
    }
    spinlock_release_irqrestore(&s_console_lock, flags);
}

void console_get_cursor(int *out_col, int *out_row)
{
    uint64_t flags = spinlock_acquire_irqsave(&s_console_lock);
    if (out_col != NULL)
    {
        *out_col = s_cursor_x;
    }
    if (out_row != NULL)
    {
        *out_row = s_cursor_y;
    }
    spinlock_release_irqrestore(&s_console_lock, flags);
}

int console_get_cols(void)
{
    return s_cols;
}

int console_get_rows(void)
{
    return s_rows;
}

void console_flush(void)
{
    if (!s_console_initialized)
    {
        return;
    }

    uint64_t flags = spinlock_acquire_irqsave(&s_console_lock);
    framebuffer_swap(s_framebuffer);
    spinlock_release_irqrestore(&s_console_lock, flags);
}
