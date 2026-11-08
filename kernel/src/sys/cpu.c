#include "sys/cpu.h"

// +------------------------------------+
// | MODEL SPECIFIC REGISTERS FUNCTIONS |
// +------------------------------------+

void wrmsr(uint64_t msr, uint64_t value)
{
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

uint64_t rdmsr(uint64_t msr)
{
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

bool pat_enable_write_combining(void)
{
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    if ((edx & CPUID_EDX_PAT) == 0)
    {
        return false;
    }

    uint64_t pat = rdmsr(MSR_PAT);
    // Set PA2 (bits 18:16) to 0x01 (Write-Combining)
    pat &= ~(0x7ULL << 16);
    pat |= (0x1ULL << 16);
    wrmsr(MSR_PAT, pat);
    return true;
}

// +-----------------------+
// | CPU-RELATED FUNCTIONS |
// +-----------------------+

void cpuid_ex(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    uint32_t a = 0, b = 0, c = 0, d = 0;
    asm volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf), "c"(subleaf));
    if (eax != (void *)0)
    {
        *eax = a;
    }
    if (ebx != (void *)0)
    {
        *ebx = b;
    }
    if (ecx != (void *)0)
    {
        *ecx = c;
    }
    if (edx != (void *)0)
    {
        *edx = d;
    }
}

void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    cpuid_ex(leaf, 0, eax, ebx, ecx, edx);
}

uint64_t rdtsc(void)
{
    uint32_t low, high;
    asm volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

uint64_t read_cr0(void)
{
    uint64_t val;
    asm volatile("mov %%cr0, %0" : "=r"(val));
    return val;
}

uint64_t read_cr2(void)
{
    uint64_t val;
    asm volatile("mov %%cr2, %0" : "=r"(val));
    return val;
}

uint64_t read_cr3(void)
{
    uint64_t val;
    asm volatile("mov %%cr3, %0" : "=r"(val));
    return val;
}

void write_cr3(uint64_t val)
{
    asm volatile("mov %0, %%cr3" : : "r"(val) : "memory");
}