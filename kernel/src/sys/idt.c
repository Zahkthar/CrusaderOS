#include "sys/idt.h"

#include "drivers/serial.h"
#include "klibc/stdio.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "sys/cpu.h"
#include "sys/gdt.h"

// +--------------------+
// | INTERNAL VARIABLES |
// +--------------------+

static IDTEntry g_IDT[IDT_ENTRIES_COUNT] __attribute__((aligned(16)));
static IDTHandler g_interrupt_handlers[IDT_ENTRIES_COUNT];

extern void isr_stub_table(void);

// +--------------------+
// | ASSEMBLY ISR STUBS |
// +--------------------+

// 256 ISR stubs aligned on 16-byte boundaries.
// Vectors pushing an error code: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30.
// All other vectors push a dummy error code (0).
__asm__(".text\n"
        ".altmacro\n"
        ".macro DEFINE_ISR i\n"
        "    .balign 16\n"
        "    isr_\\i:\n"
        "    .if \\i == 8 || \\i == 10 || \\i == 11 || \\i == 12 || \\i == 13 || \\i == 14 || \\i == 17 || \\i == 21 "
        "|| \\i == "
        "29 || \\i == 30\n"
        "        push $\\i\n"
        "        jmp isr_common_stub\n"
        "    .else\n"
        "        push $0\n"
        "        push $\\i\n"
        "        jmp isr_common_stub\n"
        "    .endif\n"
        ".endm\n"
        ".global isr_stub_table\n"
        ".balign 16\n"
        "isr_stub_table:\n"
        ".set i, 0\n"
        ".rept 256\n"
        "    DEFINE_ISR %i\n"
        "    .set i, i + 1\n"
        ".endr\n"
        "isr_common_stub:\n"
        "    cld\n"
        "    push %rax\n"
        "    push %rbx\n"
        "    push %rcx\n"
        "    push %rdx\n"
        "    push %rsi\n"
        "    push %rdi\n"
        "    push %rbp\n"
        "    push %r8\n"
        "    push %r9\n"
        "    push %r10\n"
        "    push %r11\n"
        "    push %r12\n"
        "    push %r13\n"
        "    push %r14\n"
        "    push %r15\n"
        "    testb $3, 144(%rsp)\n"
        "    jz 3f\n"
        "    swapgs\n"
        "3:\n"
        "    mov %rsp, %rdi\n"
        "    test $15, %rsp\n"
        "    jz 1f\n"
        "    sub $8, %rsp\n"
        "    call idt_dispatch_interrupt\n"
        "    add $8, %rsp\n"
        "    jmp 2f\n"
        "1:\n"
        "    call idt_dispatch_interrupt\n"
        "2:\n"
        "    pop %r15\n"
        "    pop %r14\n"
        "    pop %r13\n"
        "    pop %r12\n"
        "    pop %r11\n"
        "    pop %r10\n"
        "    pop %r9\n"
        "    pop %r8\n"
        "    pop %rbp\n"
        "    pop %rdi\n"
        "    pop %rsi\n"
        "    pop %rdx\n"
        "    pop %rcx\n"
        "    pop %rbx\n"
        "    pop %rax\n"
        "    add $16, %rsp\n"
        "    testb $3, 8(%rsp)\n"
        "    jz 4f\n"
        "    swapgs\n"
        "4:\n"
        "    iretq\n");

// +------------------+
// | HELPER FUNCTIONS |
// +------------------+

static const char *idt_get_exception_name(uint64_t vector)
{
    switch (vector)
    {
    case 0:
        return "#DE Divide Error";
    case 1:
        return "#DB Debug Exception";
    case 2:
        return "NMI Non-Maskable Interrupt";
    case 3:
        return "#BP Breakpoint";
    case 4:
        return "#OF Overflow";
    case 5:
        return "#BR Bound Range Exceeded";
    case 6:
        return "#UD Invalid Opcode";
    case 7:
        return "#NM Device Not Available";
    case 8:
        return "#DF Double Fault";
    case 9:
        return "Coprocessor Segment Overrun";
    case 10:
        return "#TS Invalid TSS";
    case 11:
        return "#NP Segment Not Present";
    case 12:
        return "#SS Stack-Segment Fault";
    case 13:
        return "#GP General Protection Fault";
    case 14:
        return "#PF Page Fault";
    case 15:
        return "Reserved Exception";
    case 16:
        return "#MF x87 FPU Floating-Point Error";
    case 17:
        return "#AC Alignment Check";
    case 18:
        return "#MC Machine Check";
    case 19:
        return "#XM SIMD Floating-Point Exception";
    case 20:
        return "#VE Virtualization Exception";
    case 21:
        return "#CP Control Protection Exception";
    case 28:
        return "#HV Hypervisor Injection Exception";
    case 29:
        return "#VC VMM Communication Exception";
    case 30:
        return "#SX Security Exception";
    default:
        return "Reserved / Unknown Exception";
    }
}

static void idt_panic_dump(InterruptRegisters *regs)
{
    kprintf("\r\n==================== KERNEL PANIC: CPU EXCEPTION ====================\r\n");
    kprintf("Exception: %s (Vector %d)\r\n", idt_get_exception_name(regs->vector), (int)regs->vector);
    kprintf("Error Code: 0x%016lx\r\n", regs->errorCode);

    if (regs->vector == 14)
    {
        uint64_t cr2 = read_cr2();
        kprintf("Faulting Address (CR2): 0x%016lx\r\n", cr2);
    }

    kprintf("\r\n--- Execution Context ---\r\n");
    kprintf("RIP   : 0x%016lx  CS    : 0x%016lx  RFLAGS: 0x%016lx\r\n", regs->rip, regs->cs, regs->rflags);
    kprintf("RSP   : 0x%016lx  SS    : 0x%016lx  RBP   : 0x%016lx\r\n", regs->rsp, regs->ss, regs->rbp);

    kprintf("\r\n--- General Purpose Registers ---\r\n");
    kprintf("RAX   : 0x%016lx  RBX   : 0x%016lx  RCX   : 0x%016lx\r\n", regs->rax, regs->rbx, regs->rcx);
    kprintf("RDX   : 0x%016lx  RSI   : 0x%016lx  RDI   : 0x%016lx\r\n", regs->rdx, regs->rsi, regs->rdi);
    kprintf("R8    : 0x%016lx  R9    : 0x%016lx  R10   : 0x%016lx\r\n", regs->r8, regs->r9, regs->r10);
    kprintf("R11   : 0x%016lx  R12   : 0x%016lx  R13   : 0x%016lx\r\n", regs->r11, regs->r12, regs->r13);
    kprintf("R14   : 0x%016lx  R15   : 0x%016lx\r\n", regs->r14, regs->r15);

    kprintf("\r\n--- Control Registers ---\r\n");
    kprintf("CR0   : 0x%016lx  CR3   : 0x%016lx\r\n", read_cr0(), read_cr3());

    kprintf("======================================================================\r\n");
    kprintf("[ CRITICAL ] Kernel halted due to unhandled CPU exception.\r\n");

    hcf();
}

// +------------------------+
// | IDT HANDLING FUNCTIONS |
// +------------------------+

void idt_set_gate(uint8_t vector, void *isr, uint8_t typeAttributes, uint8_t ist)
{
    uint64_t addr = (uint64_t)isr;

    g_IDT[vector].offsetLow = (uint16_t)(addr & 0xFFFF);
    g_IDT[vector].selector = GDT_KERNEL_CODE_SELECTOR;
    g_IDT[vector].ist = (uint8_t)(ist & 0x07);
    g_IDT[vector].typeAttributes = typeAttributes;
    g_IDT[vector].offsetMiddle = (uint16_t)((addr >> 16) & 0xFFFF);
    g_IDT[vector].offsetHigh = (uint32_t)((addr >> 32) & 0xFFFFFFFF);
    g_IDT[vector].reserved = 0;
}

void idt_register_handler(uint8_t vector, IDTHandler handler)
{
    g_interrupt_handlers[vector] = handler;
}

void idt_unregister_handler(uint8_t vector)
{
    g_interrupt_handlers[vector] = NULL;
}

void idt_dispatch_interrupt(InterruptRegisters *regs)
{
    if (g_interrupt_handlers[regs->vector] != NULL)
    {
        g_interrupt_handlers[regs->vector](regs);
    }
    else if (regs->vector < 32)
    {
        idt_panic_dump(regs);
    }
    else
    {
        kprintf("[ WARN ] Unhandled interrupt vector: %d\r\n", (int)regs->vector);
    }
}

void idt_init(void)
{
    memset(g_IDT, 0, sizeof(g_IDT));
    memset(g_interrupt_handlers, 0, sizeof(g_interrupt_handlers));

    for (int i = 0; i < IDT_ENTRIES_COUNT; i++)
    {
        void *isr = (void *)((uintptr_t)isr_stub_table + (i * 16));
        idt_set_gate((uint8_t)i, isr, IDT_ATTR_KERNEL_INTERRUPT, 0);
    }

    // Vector 8: Double Fault (#DF) uses dedicated IST1 stack to prevent Triple Fault
    idt_set_gate(8, (void *)((uintptr_t)isr_stub_table + (8 * 16)), IDT_ATTR_KERNEL_INTERRUPT, 1);

    // Allow breakpoint exception (#BP) to be triggered from user mode (Ring 3)
    idt_set_gate(3, (void *)((uintptr_t)isr_stub_table + (3 * 16)), IDT_ATTR_USER_INTERRUPT, 0);

    // Descripteur IDTR alloué localement sur la pile
    // Élimine toute relocalisation non alignée dans la section .data / .rela.dyn
    volatile IDTDescriptor idtr = {
        .size = (uint16_t)(sizeof(g_IDT) - 1),
        .base = (uint64_t)&g_IDT[0],
    };

    asm volatile("lidt (%0)" : : "r"(&idtr) : "memory");

    kprintf("[ OK ] IDT initialized (256 vectors configured, IST1 for #DF)\r\n");
}
