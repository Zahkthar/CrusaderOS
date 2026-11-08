#include "drivers/serial.h"

#include <stddef.h>

#include "klibc/string.h"
#include "sys/cpu.h"

#define MAX_COM_PORTS 8
static bool s_com_available[MAX_COM_PORTS] = {false};
static char s_last_char[MAX_COM_PORTS] = {0};

#define SERIAL_TIMEOUT 100000

static int serial_port_to_index(uint16_t comPort)
{
    switch (comPort)
    {
    case SERIAL_COM1:
        return 0;
    case SERIAL_COM2:
        return 1;
    case SERIAL_COM3:
        return 2;
    case SERIAL_COM4:
        return 3;
    case SERIAL_COM5:
        return 4;
    case SERIAL_COM6:
        return 5;
    case SERIAL_COM7:
        return 6;
    case SERIAL_COM8:
        return 7;
    default:
        return -1;
    }
}

bool serial_is_available(uint16_t comPort)
{
    int idx = serial_port_to_index(comPort);
    if (idx >= 0 && idx < MAX_COM_PORTS)
    {
        return s_com_available[idx];
    }
    return false;
}

static void setLineProtocol(uint16_t comPort, uint16_t divisorValue, uint8_t charLength, uint8_t nbStopBits,
                            uint8_t parity)
{
    // Baud rate divisor latch
    outb(SERIAL_LINE_CONTROL_REGISTER(comPort), 0x80);
    outb(SERIAL_DATA_REGISTER(comPort), (uint8_t)(divisorValue & 0x00FF));
    outb(SERIAL_INTERRUPT_ENABLE_REGISTER(comPort), (uint8_t)((divisorValue >> 8) & 0x00FF));

    uint8_t lineControlProtocol = 0;

    // Data bits
    switch (charLength)
    {
    case 5:
        break;
    case 6:
        lineControlProtocol |= (1 << 0);
        break;
    case 7:
        lineControlProtocol |= (1 << 1);
        break;
    case 8:
    default:
        lineControlProtocol |= (1 << 0);
        lineControlProtocol |= (1 << 1);
        break;
    }

    // Stop bits
    lineControlProtocol |= (uint8_t)(nbStopBits << 2);

    // Parity
    lineControlProtocol |= parity;

    outb(SERIAL_LINE_CONTROL_REGISTER(comPort), lineControlProtocol);
}

void serial_putchar(uint16_t comPort, char character)
{
    if (!serial_is_available(comPort))
    {
        return;
    }

    int idx = serial_port_to_index(comPort);

    // If character is '\n' and wasn't preceded by '\r', emit '\r' first (CRLF)
    if (character == '\n' && (idx < 0 || s_last_char[idx] != '\r'))
    {
        uint32_t cr_timeout = SERIAL_TIMEOUT;
        while ((serial_getLineStatus(comPort) & SERIAL_LINE_STATUS_TRANSMITTER_HOLDING_REGISTER_EMPTY) == 0)
        {
            if (--cr_timeout == 0)
            {
                return;
            }
        }
        outb(SERIAL_DATA_REGISTER(comPort), '\r');
    }

    uint32_t timeout = SERIAL_TIMEOUT;
    while ((serial_getLineStatus(comPort) & SERIAL_LINE_STATUS_TRANSMITTER_HOLDING_REGISTER_EMPTY) == 0)
    {
        if (--timeout == 0)
        {
            return;
        }
    }
    outb(SERIAL_DATA_REGISTER(comPort), (uint8_t)character);

    if (idx >= 0 && idx < MAX_COM_PORTS)
    {
        s_last_char[idx] = character;
    }
}

int serial_init(uint16_t comPort)
{
    int idx = serial_port_to_index(comPort);
    if (idx >= 0 && idx < MAX_COM_PORTS)
    {
        s_com_available[idx] = false;
        s_last_char[idx] = 0;
    }

    outb(SERIAL_INTERRUPT_ENABLE_REGISTER(comPort), 0x00); // Disable all interrupts
    setLineProtocol(comPort, 1, 8, 0, SERIAL_PARITY_NONE); // 115200 bauds, 8 bits per char, 1 stop bit, no parity
    outb(SERIAL_FIFO_CONTROL_REGISTER(comPort), 0xC7);     // Enable FIFO, clear them, with 14-byte threshold

    outb(SERIAL_MODEM_CONTROL_REGISTER(comPort), 0x1E);    // Set in loopback mode, test the serial chip
    outb(SERIAL_DATA_REGISTER(comPort), 0xAE);             // Send test byte 0xAE
    io_wait();

    // Wait until byte is ready to read in loopback
    uint32_t loopback_timeout = SERIAL_TIMEOUT;
    while ((serial_getLineStatus(comPort) & SERIAL_LINE_STATUS_DATA_READY) == 0)
    {
        if (--loopback_timeout == 0)
        {
            return 1;
        }
    }

    if (inb(SERIAL_DATA_REGISTER(comPort)) != 0xAE)
    {
        return 1;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(SERIAL_MODEM_CONTROL_REGISTER(comPort), 0x0F);

    if (idx >= 0 && idx < MAX_COM_PORTS)
    {
        s_com_available[idx] = true;
    }
    return 0;
}

uint8_t serial_getLineStatus(uint16_t comPort)
{
    return inb(SERIAL_LINE_STATUS_REGISTER(comPort));
}

uint8_t serial_read(uint16_t comPort)
{
    if (!serial_is_available(comPort))
    {
        return 0;
    }

    uint32_t timeout = SERIAL_TIMEOUT;
    while ((serial_getLineStatus(comPort) & SERIAL_LINE_STATUS_DATA_READY) == 0)
    {
        if (--timeout == 0)
        {
            return 0;
        }
    }
    return inb(SERIAL_DATA_REGISTER(comPort));
}

void serial_write(uint16_t comPort, const char *str)
{
    if (str == NULL || !serial_is_available(comPort))
    {
        return;
    }

    while (*str != '\0')
    {
        serial_putchar(comPort, *str);
        str++;
    }
}