#include "sys/cpu.h"

// +------------------------------------+
// | MODEL SPECIFIC REGISTERS FUNCTIONS |
// +------------------------------------+

void wrmsr(uint64_t msr, uint64_t value)
{
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile (
        "wrmsr"
        :
        : "c"(msr), "a"(low), "d"(high)
    );
}

uint64_t rdmsr(uint64_t msr)
{
    uint32_t low, high;
    asm volatile (
        "rdmsr"
        : "=a"(low), "=d"(high)
        : "c"(msr)
    );
	return ((uint64_t)high << 32) | low;
}

// +-----------------------+
// | CPU-RELATED FUNCTIONS |
// +-----------------------+

void cpuid(uint32_t reg, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    asm volatile("cpuid" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "0"(reg));
}

uint64_t rdtsc(void)
{
    uint32_t low, high;
    asm volatile("rdtsc":"=a"(low),"=d"(high));
    return ((uint64_t)high << 32) | low;
}

uint64_t read_cr0(void)
{
    uint64_t val;
    asm volatile ( "mov %%cr0, %0" : "=r"(val) );
    return val;
}

// +-----+
// | I/O |
// +-----+

void outb(uint16_t port, uint8_t data)
{
    asm volatile("outb %b0, %w1" : : "a" (data), "Nd" (port));
}

void outw(uint16_t port, uint16_t data)
{
    asm volatile("outw %w0, %w1" : : "a" (data), "Nd" (port));
}

void outl(uint16_t port, uint32_t data)
{
    asm volatile("outl %0, %w1" : : "a" (data), "Nd" (port));
}

uint8_t inb(uint16_t port)
{
    uint8_t data;
    asm volatile("inb %w1, %b0" : "=a" (data) : "Nd" (port));
    return data;
}

uint16_t inw(uint16_t port)
{
    uint16_t data;
    asm volatile("inw %w1, %w0" : "=a" (data) : "Nd" (port));
    return data;
}


uint32_t inl(uint16_t port)
{
    uint32_t data;
    asm volatile("inl %w1, %0" : "=a" (data) : "Nd" (port));
    return data;
}

void io_wait()
{
    asm volatile("jmp 1f;1:jmp 1f;1:");
}

// +-------------------------+
// | INTERRUPTIONS AND PAUSE |
// +-------------------------+

// Will be moved to a "interrupts.c" file
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