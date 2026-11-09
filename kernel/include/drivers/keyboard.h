#ifndef DRIVER_KEYBOARD_H_INCLUDED
#define DRIVER_KEYBOARD_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

// 8042 PS/2 Controller I/O Ports
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64
#define KEYBOARD_COMMAND_PORT 0x64

// 8042 Status Register Flags
#define KEYBOARD_STATUS_OUTPUT_BUFFER_FULL (1 << 0)
#define KEYBOARD_STATUS_INPUT_BUFFER_FULL  (1 << 1)

// Interrupt vector
#define KEYBOARD_IRQ_VECTOR 0x21

// Ring buffer capacity (must be a power of 2 for fast wrapping)
#define KEYBOARD_BUFFER_CAPACITY 256

// Modifier Key Flags
#define KEY_MOD_SHIFT (1 << 0)
#define KEY_MOD_CTRL  (1 << 1)
#define KEY_MOD_ALT   (1 << 2)
#define KEY_MOD_CAPS  (1 << 3)

// Special ASCII Key codes
#define KEY_NONE      0x00
#define KEY_ESCAPE    0x1B
#define KEY_BACKSPACE 0x08
#define KEY_TAB       0x09
#define KEY_ENTER     '\n'

// +------------+
// | STRUCTURES |
// +------------+

typedef enum Keymap
{
    KEYMAP_QWERTY = 0,
    KEYMAP_AZERTY = 1
} Keymap;

typedef struct KeyEvent
{
    uint8_t scancode;
    char ascii;
    bool pressed;
    uint8_t modifiers;
} KeyEvent;

// +-----------+
// | FUNCTIONS |
// +-----------+

bool keyboard_init(void);

void keyboard_set_keymap(Keymap map);
Keymap keyboard_get_keymap(void);

bool keyboard_has_event(void);
bool keyboard_get_event(KeyEvent *event);

bool keyboard_has_char(void);
char keyboard_getchar(void);
char keyboard_getchar_nonblock(void);

uint8_t keyboard_get_modifiers(void);
void keyboard_flush(void);

#endif // DRIVER_KEYBOARD_H_INCLUDED
