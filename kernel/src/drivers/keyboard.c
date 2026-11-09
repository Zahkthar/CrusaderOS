#include "drivers/keyboard.h"

#include "klibc/stdio.h"
#include "sys/apic.h"
#include "sys/cpu.h"
#include "sys/idt.h"
#include "sys/pic.h"
#include "sys/spinlock.h"

// +--------------------+
// | SCANCODE TABLES    |
// +--------------------+

// US QWERTY (Scancode Set 1) - Unshifted
static const char s_scancode_to_ascii_qwerty_normal[128] = {
    0,   27,   '1', '2', '3', '4', '5', '6',  '7', '8', '9', '0', '-', '=', '\b', '\t', 'q', 'w', 'e', 'r',  't',
    'y', 'u',  'i', 'o', 'p', '[', ']', '\n', 0,   'a', 's', 'd', 'f', 'g', 'h',  'j',  'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n',  'm', ',', '.', '/', 0,   '*', 0,    ' ',  0,   0,   0,   0,    0,
    0,   0,    0,   0,   0,   0,   0,   0,    0,   0,   '-', 0,   0,   0,   '+',  0,    0,   0,   0,   0,    0,
    0,   0,    0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,   0,    0,
    0,   0,    0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,   0};

// US QWERTY (Scancode Set 1) - Shifted
static const char s_scancode_to_ascii_qwerty_shifted[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^',  '&', '*', '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R', 'T',
    'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,   'A', 'S', 'D', 'F', 'G', 'H',  'J',  'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N',  'M', '<', '>', '?', 0,   '*', 0,    ' ',  0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   '-', 0,   0,   0,   '+',  0,    0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,   0};

// French AZERTY (Scancode Set 1) - Unshifted
static const char s_scancode_to_ascii_azerty_normal[128] = {
    0,   27,  '&', 'e', '"', '\'', '(', '-',  'e', '_', 'c', 'a', ')', '=', '\b', '\t', 'a', 'z', 'e', 'r', 't',
    'y', 'u', 'i', 'o', 'p', '^',  '$', '\n', 0,   'q', 's', 'd', 'f', 'g', 'h',  'j',  'k', 'l', 'm', '%', '*',
    0,   '*', 'w', 'x', 'c', 'v',  'b', 'n',  ',', ';', ':', '!', 0,   '*', 0,    ' ',  0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,    0,   0,    0,   0,   '-', 0,   0,   0,   '+',  0,    0,   0,   0,   0,   0,
    '<', 0,   0,   0,   0,   0,    0,   0,    0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,    0,   0,    0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,   0};

// French AZERTY (Scancode Set 1) - Shifted
static const char s_scancode_to_ascii_azerty_shifted[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6',  '7', '8', '9', '0', 'o', '+', '\b', '\t', 'A', 'Z', 'E', 'R', 'T',
    'Y', 'U', 'I', 'O', 'P', '"', '#', '\n', 0,   'Q', 'S', 'D', 'F', 'G', 'H',  'J',  'K', 'L', 'M', 'u', 'u',
    0,   'u', 'W', 'X', 'C', 'V', 'B', 'N',  '?', '.', '/', 's', 0,   '*', 0,    ' ',  0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   '-', 0,   0,   0,   '+',  0,    0,   0,   0,   0,   0,
    '>', 0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,    0,    0,   0,   0,   0};

// +--------------------+
// | INTERNAL VARIABLES |
// +--------------------+

static Keymap s_current_keymap = KEYMAP_QWERTY;
static bool s_extended = false;
static bool s_shift_l = false;
static bool s_shift_r = false;
static bool s_ctrl = false;
static bool s_alt = false;
static bool s_caps_lock = false;

static KeyEvent s_ring_buffer[KEYBOARD_BUFFER_CAPACITY];
static size_t s_head = 0;
static size_t s_tail = 0;
static size_t s_count = 0;
static Spinlock s_keyboard_lock = SPINLOCK_INIT;
static bool s_keyboard_initialized = false;

// +------------------+
// | HELPER FUNCTIONS |
// +------------------+

static inline uint8_t get_current_modifiers(void)
{
    uint8_t mods = 0;
    if (s_shift_l || s_shift_r)
    {
        mods |= KEY_MOD_SHIFT;
    }
    if (s_ctrl)
    {
        mods |= KEY_MOD_CTRL;
    }
    if (s_alt)
    {
        mods |= KEY_MOD_ALT;
    }
    if (s_caps_lock)
    {
        mods |= KEY_MOD_CAPS;
    }
    return mods;
}

static char translate_scancode(uint8_t scancode, bool shift, bool caps)
{
    if (scancode >= 128)
    {
        return 0;
    }

    const char *normal_table =
        (s_current_keymap == KEYMAP_AZERTY) ? s_scancode_to_ascii_azerty_normal : s_scancode_to_ascii_qwerty_normal;

    const char *shifted_table =
        (s_current_keymap == KEYMAP_AZERTY) ? s_scancode_to_ascii_azerty_shifted : s_scancode_to_ascii_qwerty_shifted;

    char norm = normal_table[scancode];
    char shft = shifted_table[scancode];

    if (norm >= 'a' && norm <= 'z')
    {
        bool upper = (shift ^ caps);
        return upper ? shft : norm;
    }

    return shift ? shft : norm;
}

static void keyboard_interrupt_handler(InterruptRegisters *regs)
{
    (void)regs;

    uint8_t status = inb(KEYBOARD_STATUS_PORT);
    if ((status & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL) == 0)
    {
        pic_send_eoi(1);
        return;
    }

    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode == 0xE0)
    {
        s_extended = true;
        pic_send_eoi(1);
        return;
    }

    bool released = (scancode & 0x80) != 0;
    uint8_t code = scancode & 0x7F;

    // Track modifiers
    if (code == 0x2A) // Left Shift
    {
        s_shift_l = !released;
    }
    else if (code == 0x36) // Right Shift
    {
        s_shift_r = !released;
    }
    else if (code == 0x1D) // Control
    {
        s_ctrl = !released;
    }
    else if (code == 0x38) // Alt
    {
        s_alt = !released;
    }
    else if (code == 0x3A && !released) // Caps Lock toggle
    {
        s_caps_lock = !s_caps_lock;
    }

    bool shift = s_shift_l || s_shift_r;
    char ascii = 0;
    if (!released && !s_extended)
    {
        ascii = translate_scancode(code, shift, s_caps_lock);
    }

    KeyEvent event;
    event.scancode = scancode;
    event.ascii = ascii;
    event.pressed = !released;
    event.modifiers = get_current_modifiers();

    // Push into protected ring buffer
    uint64_t flags = spinlock_acquire_irqsave(&s_keyboard_lock);
    if (s_count < KEYBOARD_BUFFER_CAPACITY)
    {
        s_ring_buffer[s_tail] = event;
        s_tail = (s_tail + 1) % KEYBOARD_BUFFER_CAPACITY;
        s_count++;
    }
    spinlock_release_irqrestore(&s_keyboard_lock, flags);

    s_extended = false;

    // Send EOI to PIC for legacy IRQ 1
    pic_send_eoi(1);
}

// +-----------+
// | FUNCTIONS |
// +-----------+

bool keyboard_init(void)
{
    if (s_keyboard_initialized)
    {
        return true;
    }

    // Flush any pending data in 8042 output buffer
    while (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL)
    {
        inb(KEYBOARD_DATA_PORT);
    }

    // Disable both PS/2 ports
    outb(KEYBOARD_COMMAND_PORT, 0xAD);
    io_wait();
    outb(KEYBOARD_COMMAND_PORT, 0xA7);
    io_wait();

    // Flush buffer again
    while (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL)
    {
        inb(KEYBOARD_DATA_PORT);
    }

    // Read Controller Configuration Byte
    outb(KEYBOARD_COMMAND_PORT, 0x20);
    io_wait();

    uint8_t config = 0;
    for (int i = 0; i < 10000; i++)
    {
        if (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL)
        {
            config = inb(KEYBOARD_DATA_PORT);
            break;
        }
        io_wait();
    }

    // Enable Port 1 interrupt (bit 0), enable clock (clear bit 4), enable translation (bit 6)
    config |= 0x01;
    config &= ~0x10;
    config |= 0x40;

    // Write modified Configuration Byte
    outb(KEYBOARD_COMMAND_PORT, 0x60);
    io_wait();
    for (int i = 0; i < 10000; i++)
    {
        if ((inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_INPUT_BUFFER_FULL) == 0)
        {
            break;
        }
        io_wait();
    }
    outb(KEYBOARD_DATA_PORT, config);
    io_wait();

    // Enable PS/2 Port 1
    outb(KEYBOARD_COMMAND_PORT, 0xAE);
    io_wait();

    // Send Enable Scanning command (0xF4) to keyboard device
    for (int i = 0; i < 10000; i++)
    {
        if ((inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_INPUT_BUFFER_FULL) == 0)
        {
            break;
        }
        io_wait();
    }
    outb(KEYBOARD_DATA_PORT, 0xF4);
    io_wait();

    // Drain any immediate ACK (0xFA)
    for (int i = 0; i < 10000; i++)
    {
        if (inb(KEYBOARD_STATUS_PORT) & KEYBOARD_STATUS_OUTPUT_BUFFER_FULL)
        {
            inb(KEYBOARD_DATA_PORT);
            break;
        }
        io_wait();
    }

    // Register Keyboard IRQ 1 handler in IDT (Vector 0x21)
    idt_register_handler(KEYBOARD_IRQ_VECTOR, keyboard_interrupt_handler);

    // Unmask IRQ 1 on the legacy 8259 PIC
    pic_unmask_irq(1);

    s_keyboard_initialized = true;
    kprintf("[ OK ] PS/2 Keyboard Controller initialized (IRQ 1 / Vector 0x21, QWERTY default)\r\n");
    return true;
}

void keyboard_set_keymap(Keymap map)
{
    uint64_t flags = spinlock_acquire_irqsave(&s_keyboard_lock);
    s_current_keymap = map;
    spinlock_release_irqrestore(&s_keyboard_lock, flags);
}

Keymap keyboard_get_keymap(void)
{
    return s_current_keymap;
}

bool keyboard_has_event(void)
{
    uint64_t flags = spinlock_acquire_irqsave(&s_keyboard_lock);
    bool has = (s_count > 0);
    spinlock_release_irqrestore(&s_keyboard_lock, flags);
    return has;
}

bool keyboard_get_event(KeyEvent *event)
{
    if (event == NULL)
    {
        return false;
    }

    uint64_t flags = spinlock_acquire_irqsave(&s_keyboard_lock);
    if (s_count == 0)
    {
        spinlock_release_irqrestore(&s_keyboard_lock, flags);
        return false;
    }

    *event = s_ring_buffer[s_head];
    s_head = (s_head + 1) % KEYBOARD_BUFFER_CAPACITY;
    s_count--;

    spinlock_release_irqrestore(&s_keyboard_lock, flags);
    return true;
}

bool keyboard_has_char(void)
{
    uint64_t flags = spinlock_acquire_irqsave(&s_keyboard_lock);
    size_t idx = s_head;
    for (size_t i = 0; i < s_count; i++)
    {
        if (s_ring_buffer[idx].pressed && s_ring_buffer[idx].ascii != '\0')
        {
            spinlock_release_irqrestore(&s_keyboard_lock, flags);
            return true;
        }
        idx = (idx + 1) % KEYBOARD_BUFFER_CAPACITY;
    }
    spinlock_release_irqrestore(&s_keyboard_lock, flags);
    return false;
}

char keyboard_getchar(void)
{
    for (;;)
    {
        KeyEvent ev;
        while (keyboard_get_event(&ev))
        {
            if (ev.pressed && ev.ascii != '\0')
            {
                return ev.ascii;
            }
        }
        hlt();
    }
}

char keyboard_getchar_nonblock(void)
{
    KeyEvent ev;
    while (keyboard_get_event(&ev))
    {
        if (ev.pressed && ev.ascii != '\0')
        {
            return ev.ascii;
        }
    }
    return '\0';
}

uint8_t keyboard_get_modifiers(void)
{
    return get_current_modifiers();
}

void keyboard_flush(void)
{
    uint64_t flags = spinlock_acquire_irqsave(&s_keyboard_lock);
    s_head = 0;
    s_tail = 0;
    s_count = 0;
    spinlock_release_irqrestore(&s_keyboard_lock, flags);
}
