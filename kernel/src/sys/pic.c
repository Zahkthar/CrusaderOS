#include "sys/pic.h"

#include "klibc/stdio.h"
#include "sys/cpu.h"

// +-----------+
// | FUNCTIONS |
// +-----------+

void pic_remap(uint8_t offset1, uint8_t offset2)
{
    // Save interrupt masks
    uint8_t a1 = inb(PIC1_DATA);
    uint8_t a2 = inb(PIC2_DATA);

    // ICW1: Start initialization sequence in cascade mode
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();

    // ICW2: Set vector offsets
    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();

    // ICW3: Tell Master PIC that Slave PIC is at IRQ2 (0000 0100)
    outb(PIC1_DATA, 0x04);
    io_wait();
    // Tell Slave PIC its cascade identity (0000 0010)
    outb(PIC2_DATA, 0x02);
    io_wait();

    // ICW4: Set 8086/88 (MCS-80/85) mode
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    // Restore saved masks
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

void pic_disable(void)
{
    // Remap PIC vectors to 0x20-0x2F to ensure spurious IRQs (7, 15)
    // don't conflict with CPU exceptions 0x00-0x1F (e.g., #DF vector 8)
    pic_remap(0x20, 0x28);

    // Mask all interrupt lines on both master and slave PICs
    outb(PIC1_DATA, 0xFF);
    io_wait();
    outb(PIC2_DATA, 0xFF);
    io_wait();

    kprintf("[ OK ] Legacy 8259 PIC disabled and masked\r\n");
}

void pic_unmask_irq(uint8_t irq)
{
    uint16_t port;
    if (irq < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = inb(port) & (uint8_t)~(1 << irq);
    outb(port, value);
    io_wait();
}

void pic_mask_irq(uint8_t irq)
{
    uint16_t port;
    if (irq < 8)
    {
        port = PIC1_DATA;
    }
    else
    {
        port = PIC2_DATA;
        irq -= 8;
    }
    uint8_t value = inb(port) | (uint8_t)(1 << irq);
    outb(port, value);
    io_wait();
}

void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8)
    {
        outb(PIC2_COMMAND, PIC_EOI);
    }
    outb(PIC1_COMMAND, PIC_EOI);
}
