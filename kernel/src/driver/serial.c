#include "driver/serial.h"

static void setLineProtocol(uint16_t comPort, uint8_t divisorValue, uint16_t charLength, uint8_t nbStopBits, uint8_t parity)
{
    // baudRate
    outb(SERIAL_LINE_CONTROL_REGISTER(comPort), 0x80);
    outb(SERIAL_DATA_REGISTER(comPort)    ,  divisorValue       & 0x00FF);
    outb(SERIAL_DATA_REGISTER(comPort) + 1, (divisorValue >> 8) & 0x00FF);

    uint8_t lineControlProtocol = 0;

    // Data bits
    switch (charLength)
    {
    case 5: // 0b00 
        break;
    case 6: // 0b01
        lineControlProtocol |= (1 << 0);
        break;
    case 7: // 0b10
        lineControlProtocol |= (1 << 1);
        break;
    case 8: // 0b11
        lineControlProtocol |= (1 << 0);
        lineControlProtocol |= (1 << 1);
        break;
    }

    // Stop bits
    lineControlProtocol |= (nbStopBits << 2);

    // Parity
    lineControlProtocol |= parity;

    outb(SERIAL_LINE_CONTROL_REGISTER(comPort), lineControlProtocol);
}

uint8_t serial_getLineStatus(uint16_t comPort)
{
    return inb(SERIAL_LINE_STATUS_REGISTER(comPort));
}

uint32_t serial_read(uint16_t comPort)
{
    while ((serial_getLineStatus(comPort) & DATA_READY) == 0);
    return inb(SERIAL_DATA_REGISTER(comPort));
}

static void putchar(uint16_t comPort, char character)
{
    while ((serial_getLineStatus(comPort) & TRANSMITTER_HOLDING_REGISTER_EMPTY) == 0);
    outb(comPort, character);
}

void serial_write(uint16_t comPort, const char *str)
{
    size_t strLen = strlen(str);
    for (size_t i = 0; i < strLen; i++)
    {
        putchar(comPort, str[i]);
    }
}

int serial_init(uint16_t comPort)
{
    outb(SERIAL_INTERRUPT_ENABLE_REGISTER(comPort), 0x00); // Disable all interrupts
    setLineProtocol(comPort, 1, 8, 0, SERIAL_PARITY_NONE); // 115200 bauds, 8 bits per char, 1 stop bit, no parity
    outb(SERIAL_FIFO_CONTROL_REGISTER(comPort), 0xC7);     // Enable FIFO, clear them, with 14-byte threshold
    outb(SERIAL_MODEM_CONTROL_REGISTER(comPort), 0x0B);    // IRQs enabled, RTS/DSR set

    outb(SERIAL_MODEM_CONTROL_REGISTER(comPort), 0x1E);    // Set in loopback mode, test the serial chip
    outb(SERIAL_DATA_REGISTER(comPort), 0xAE);             // Test serial chip (send byte 0xAE and check if serial returns same byte)
    if (inb(SERIAL_DATA_REGISTER(comPort) + 0) != 0xAE)
    {
        return 1;
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(SERIAL_MODEM_CONTROL_REGISTER(comPort), 0x0F);
    return 0;
}