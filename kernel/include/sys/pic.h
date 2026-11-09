#ifndef SYS_PIC_H_INCLUDED
#define SYS_PIC_H_INCLUDED

#include <stdint.h>

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define PIC_EOI 0x20

// +-----------+
// | FUNCTIONS |
// +-----------+

void pic_remap(uint8_t offset1, uint8_t offset2);
void pic_disable(void);
void pic_send_eoi(uint8_t irq);

#endif // SYS_PIC_H_INCLUDED
