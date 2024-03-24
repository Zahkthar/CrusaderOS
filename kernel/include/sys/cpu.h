#ifndef SYS_CPU_H_INCLUDED
#define SYS_CPU_H_INCLUDED

#include <stdint.h>
#include <stdbool.h>

// +-----------------------------------------------+
// | MODEL SPECIFIC REGISTERS FUNCTIONS AND MACROS |
// +-----------------------------------------------+

// Model Specific Registers
#define MSR_PAT             0x0277

#define MSR_FS_BASE         0xC0000100
#define MSR_GS_BASE         0xC0000101
#define MSR_KERN_GS_BASE    0xC0000102

#define MSR_EFER            0xC0000080

// https://wiki.osdev.org/SYSENTER
#define MSR_STAR            0xC0000081 
#define MSR_LSTAR           0xC0000082 // The kernel's RIP SYSCALL entry for 64 bit software.
#define MSR_CSTAR           0xC0000083 // The kernel's RIP for SYSCALL in compatibility mode.
#define MSR_SFMASK          0xC0000084 // The low 32 bits are the SYSCALL flag mask. If a bit in this is set, the corresponding bit in rFLAGS is cleared.

void wrmsr(uint64_t msr, uint64_t value);
uint64_t rdmsr(uint64_t msr);

// +----------------------------------+
// | CPU-RELATED FUNCTIONS AND MACROS |
// +----------------------------------+

// Function 0x01
#define CPUID_ECX_SSE3        (1 << 0)    // Streaming SIMD Extensions 3
#define CPUID_ECX_PCLMULQDQ   (1 << 1)    // PCLMULQDQ Instruction
#define CPUID_ECX_DTES64      (1 << 2)    // 64-Bit Debug Store Area
#define CPUID_ECX_MONITOR     (1 << 3)    // MONITOR/MWAIT
#define CPUID_ECX_DS_CPL      (1 << 4)    // CPL Qualified Debug Store
#define CPUID_ECX_VMX         (1 << 5)    // Virtual Machine Extensions
#define CPUID_ECX_SMX         (1 << 6)    // Safer Mode Extensions
#define CPUID_ECX_EST         (1 << 7)    // Enhanced SpeedStep Technology
#define CPUID_ECX_TM2         (1 << 8)    // Thermal Monitor 2
#define CPUID_ECX_SSSE3       (1 << 9)    // Supplemental Streaming SIMD Extensions 3
#define CPUID_ECX_CNXT_ID     (1 << 10)   // L1 Context ID
#define CPUID_ECX_FMA         (1 << 12)   // Fused Multiply Add
#define CPUID_ECX_CX16        (1 << 13)   // CMPXCHG16B Instruction
#define CPUID_ECX_XTPR        (1 << 14)   // xTPR Update Control
#define CPUID_ECX_PDCM        (1 << 15)   // Perf/Debug Capability MSR
#define CPUID_ECX_PCID        (1 << 17)   // Process-context Identifiers
#define CPUID_ECX_DCA         (1 << 18)   // Direct Cache Access
#define CPUID_ECX_SSE41       (1 << 19)   // Streaming SIMD Extensions 4.1
#define CPUID_ECX_SSE42       (1 << 20)   // Streaming SIMD Extensions 4.2
#define CPUID_ECX_X2APIC      (1 << 21)   // Extended xAPIC Support
#define CPUID_ECX_MOVBE       (1 << 22)   // MOVBE Instruction
#define CPUID_ECX_POPCNT      (1 << 23)   // POPCNT Instruction
#define CPUID_ECX_TSC         (1 << 24)   // Local APIC supports TSC Deadline
#define CPUID_ECX_AESNI       (1 << 25)   // AESNI Instruction
#define CPUID_ECX_XSAVE       (1 << 26)   // XSAVE/XSTOR States
#define CPUID_ECX_OSXSAVE     (1 << 27)   // OS Enabled Extended State Management
#define CPUID_ECX_AVX         (1 << 28)   // AVX Instructions
#define CPUID_ECX_F16C        (1 << 29)   // 16-bit Floating Point Instructions
#define CPUID_ECX_RDRAND      (1 << 30)   // RDRAND Instruction

#define CPUID_EDX_FPU         (1 << 0)    // Floating-Point Unit On-Chip
#define CPUID_EDX_VME         (1 << 1)    // Virtual 8086 Mode Extensions
#define CPUID_EDX_DE          (1 << 2)    // Debugging Extensions
#define CPUID_EDX_PSE         (1 << 3)    // Page Size Extension
#define CPUID_EDX_TSC         (1 << 4)    // Time Stamp Counter
#define CPUID_EDX_MSR         (1 << 5)    // Model Specific Registers
#define CPUID_EDX_PAE         (1 << 6)    // Physical Address Extension
#define CPUID_EDX_MCE         (1 << 7)    // Machine-Check Exception
#define CPUID_EDX_CX8         (1 << 8)    // CMPXCHG8 Instruction
#define CPUID_EDX_APIC        (1 << 9)    // APIC On-Chip
#define CPUID_EDX_SEP         (1 << 11)   // SYSENTER/SYSEXIT instructions
#define CPUID_EDX_MTRR        (1 << 12)   // Memory Type Range Registers
#define CPUID_EDX_PGE         (1 << 13)   // Page Global Bit
#define CPUID_EDX_MCA         (1 << 14)   // Machine-Check Architecture
#define CPUID_EDX_CMOV        (1 << 15)   // Conditional Move Instruction
#define CPUID_EDX_PAT         (1 << 16)   // Page Attribute Table
#define CPUID_EDX_PSE36       (1 << 17)   // 36-bit Page Size Extension
#define CPUID_EDX_PSN         (1 << 18)   // Processor Serial Number
#define CPUID_EDX_CLFLUSH     (1 << 19)   // CLFLUSH Instruction
#define CPUID_EDX_DS          (1 << 21)   // Debug Store
#define CPUID_EDX_ACPI        (1 << 22)   // Thermal Monitor and Software Clock Facilities
#define CPUID_EDX_MMX         (1 << 23)   // MMX Technology
#define CPUID_EDX_FXSR        (1 << 24)   // FXSAVE and FXSTOR Instructions
#define CPUID_EDX_SSE         (1 << 25)   // Streaming SIMD Extensions
#define CPUID_EDX_SSE2        (1 << 26)   // Streaming SIMD Extensions 2
#define CPUID_EDX_SS          (1 << 27)   // Self Snoop
#define CPUID_EDX_HTT         (1 << 28)   // Multi-Threading
#define CPUID_EDX_TM          (1 << 29)   // Thermal Monitor
#define CPUID_EDX_PBE         (1 << 31)   // Pending Break Enable

// Extended Function 0x01
#define CPUID_EDX_SYSCALL     (1 << 11)   // SYSCALL/SYSRET
#define CPUID_EDX_XD          (1 << 20)   // Execute Disable Bit
#define CPUID_EDX_1GB_PAGE    (1 << 26)   // 1 GB Pages
#define CPUID_EDX_RDTSCP      (1 << 27)   // RDTSCP and IA32_TSC_AUX
#define CPUID_EDX_64_BIT      (1 << 29)   // 64-bit Architecture

void cpuid(uint32_t reg, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

uint64_t rdtsc(void);
uint64_t read_cr0(void);

// +-----+
// | I/O |
// +-----+

void outb(uint16_t port, uint8_t data);
void outw(uint16_t port, uint16_t data);
void outl(uint16_t port, uint32_t data);

uint8_t inb(uint16_t port);
uint16_t inw(uint16_t port);
uint32_t inl(uint16_t port);

void io_wait();

// +-------------------------+
// | INTERRUPTIONS AND PAUSE |
// +-------------------------+

bool areInterruptsEnabled();

unsigned long save_irqdisable(void);
void irqrestore(unsigned long flags);

void cli(void); // Disable interruptions
void sti(void); // Enable interruptions

void hlt(void); // Halt
void hcf(void); // Halt and catch fire

#endif // SYS_CPU_H_INCLUDED