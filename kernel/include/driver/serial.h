#ifndef DRIVER_SERIAL_H_INCLUDED
#define DRIVER_SERIAL_H_INCLUDED

#include <stddef.h>
#include <stdbool.h>

#include "klibc/string.h"

#include "sys/cpu.h"

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

// Serial ports adresses
#define SERIAL_COM1 0x3F8
#define SERIAL_COM2 0x2F8
#define SERIAL_COM3 0x3E8
#define SERIAL_COM4 0x2E8
#define SERIAL_COM5 0x5F8
#define SERIAL_COM6 0x4F8
#define SERIAL_COM7 0x5E8
#define SERIAL_COM8 0x4E8

// Serial port registers
#define SERIAL_DATA_REGISTER(x)             (x + 0)
#define SERIAL_INTERRUPT_ENABLE_REGISTER(x) (x + 1)
#define SERIAL_FIFO_CONTROL_REGISTER(x)     (x + 2)
#define SERIAL_LINE_CONTROL_REGISTER(x)     (x + 3)
#define SERIAL_MODEM_CONTROL_REGISTER(x)    (x + 4)
#define SERIAL_LINE_STATUS_REGISTER(x)      (x + 5)
#define SERIAL_MODEM_STATUS_REGISTER(x)     (x + 6)
#define SERIAL_SCRATCH_REGISTER(x)          (x + 7)

// Line Protocol Parity
#define SERIAL_PARITY_NONE  0
#define SERIAL_PARITY_ODD   (0x1 << 3)
#define SERIAL_PARITY_EVEN  ((0x1 << 4) | (0x1 << 3))
#define SERIAL_PARITY_MARK  ((0x1 << 5) | (0x1 << 3))
#define SERIAL_PARITY_SPACE ((0x1 << 5) | (0x1 << 4) | (0x1 << 3))

// Line Status Registers Flags
#define DATA_READY                         (0 << 0)
#define OVERRUN_ERROR                      (1 << 1)
#define PARITY_ERROR                       (1 << 2)
#define FRAMING_ERROR                      (1 << 3)
#define BREAK_INDICATOR                    (1 << 4)
#define TRANSMITTER_HOLDING_REGISTER_EMPTY (1 << 5)
#define TRANSMITTER_EMPTY                  (1 << 6)
#define IMPENDING_ERROR                    (1 << 7)

// +-----------+
// | FUNCTIONS |
// +-----------+

int      serial_init         (uint16_t comPort);

uint8_t  serial_getLineStatus(uint16_t comPort);

uint32_t serial_read         (uint16_t comPort);
void     serial_write        (uint16_t comPort, const char *str);

#endif // DRIVER_SERIAL_H_INCLUDED