#include "asm.h"

// +----------------------+
// | INTERRUPTS AND PAUSE |
// +----------------------+

void cli(void)
{
    asm volatile("cli");
}

void sti(void)
{
    asm volatile("sti");
}

void hlt(void)
{
    asm volatile("hlt");
}

void hcf(void)
{
    cli();
    for (;;)
    {
        hlt();
    }
}