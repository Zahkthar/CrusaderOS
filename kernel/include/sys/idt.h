#ifndef SYS_IDT_H_INCLUDED
#define SYS_IDT_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

// IDT Gate Types
#define IDT_GATE_INTERRUPT 0x0E
#define IDT_GATE_TRAP      0x0F

// IDT Privilege Levels
#define IDT_DPL_RING0 0x00
#define IDT_DPL_RING3 0x60

// IDT Flags
#define IDT_FLAG_PRESENT 0x80

// Composite Gate Attributes
#define IDT_ATTR_KERNEL_INTERRUPT (IDT_FLAG_PRESENT | IDT_DPL_RING0 | IDT_GATE_INTERRUPT)
#define IDT_ATTR_USER_INTERRUPT   (IDT_FLAG_PRESENT | IDT_DPL_RING3 | IDT_GATE_INTERRUPT)
#define IDT_ATTR_KERNEL_TRAP      (IDT_FLAG_PRESENT | IDT_DPL_RING0 | IDT_GATE_TRAP)
#define IDT_ATTR_USER_TRAP        (IDT_FLAG_PRESENT | IDT_DPL_RING3 | IDT_GATE_TRAP)

// Total number of IDT entries
#define IDT_ENTRIES_COUNT 256

// +------------+
// | STRUCTURES |
// +------------+

// IDT Gate Entry for x86_64 (16 bytes)
typedef struct IDTEntry
{
    uint16_t offsetLow;
    uint16_t selector;
    uint8_t ist;
    uint8_t typeAttributes;
    uint16_t offsetMiddle;
    uint32_t offsetHigh;
    uint32_t reserved;
} __attribute__((packed)) IDTEntry;

// IDTR descriptor (10 bytes)
typedef struct IDTDescriptor
{
    uint16_t size;
    uint64_t base;
} __attribute__((packed)) IDTDescriptor;

// Registers pushed onto the stack during an interrupt/exception
typedef struct InterruptRegisters
{
    // General Purpose Registers (pushed by isr_common_stub)
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;

    // Pushed by ISR stub
    uint64_t vector;
    uint64_t errorCode;

    // Pushed automatically by CPU hardware
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    uint64_t rsp;
    uint64_t ss;
} __attribute__((packed)) InterruptRegisters;

typedef void (*IDTHandler)(InterruptRegisters *regs);

// +-----------+
// | FUNCTIONS |
// +-----------+

void idt_init(void);
void idt_set_gate(uint8_t vector, void *isr, uint8_t typeAttributes, uint8_t ist);
void idt_register_handler(uint8_t vector, IDTHandler handler);
void idt_unregister_handler(uint8_t vector);
void idt_dispatch_interrupt(InterruptRegisters *regs);

#endif // SYS_IDT_H_INCLUDED
