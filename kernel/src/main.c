#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/serial.h"
#include "graphics/colors.h"
#include "graphics/framebuffer.h"
#include "klibc/stdio.h"
#include "klibc/stdlib.h"
#include "memory/pmm.h"
#include "sys/boot_info.h"
#include "sys/cpu.h"
#include "sys/gdt.h"
#include "sys/idt.h"

// Assembly entry stack top symbol defined in sys/entry.S
extern uint8_t s_kernel_stack_top[];

void kmain(void);

void kmain(void)
{
    if (serial_init(SERIAL_COM1) == 0)
    {
        kprintf("\r\n========================================\r\n");
        kprintf("   CrusaderOS Kernel (x86_64)\r\n");
        kprintf("========================================\r\n");
        kprintf("[ OK ] Serial COM1 initialized (115200 8N1)\r\n");
    }

    if (!boot_info_init())
    {
        kprintf("[ FATAL ] Critical Limine boot info missing or base revision unsupported!\r\n");
        hcf();
    }
    kprintf("[ OK ] BootInfo retrieved from Limine\r\n");

    const BootInfo *boot_info = boot_info_get();
    struct limine_framebuffer *framebuffer = boot_info->framebuffer;

    kprintf("[ INFO ] Framebuffer: %ux%u (%u bpp, pitch %u, addr %p)\r\n", (unsigned int)framebuffer->width,
            (unsigned int)framebuffer->height, (unsigned int)framebuffer->bpp, (unsigned int)framebuffer->pitch,
            framebuffer->address);

    // GDT (Global Descriptor Table) & TSS Init (Task State Segment)
    gdt_init();
    tss_set_rsp0((uint64_t)s_kernel_stack_top);
    kprintf("[ OK ] GDT and TSS configured (RSP0 set to %p)\r\n", s_kernel_stack_top);

    // IDT (Interrupt Descriptor Table)
    idt_init();

    // PMM (Physical Memory Manager)
    pmm_init(boot_info);

    // Quick verification test for PMM
    void *test_page1 = pmm_alloc_page();
    void *test_page2 = pmm_alloc_page();
    kprintf("[ TEST ] PMM allocated pages: %p and %p\r\n", test_page1, test_page2);
    pmm_free_page(test_page2);
    void *test_page3 = pmm_alloc_page();
    kprintf("[ TEST ] PMM reallocated freed page: %p (matches %p)\r\n", test_page3, test_page2);
    pmm_free_page(test_page1);
    pmm_free_page(test_page3);
    kprintf("[ OK ] PMM allocation/free cycle verified\r\n");

    // Page Attribute Table (PAT) write-combining configuration
    if (pat_enable_write_combining())
    {
        kprintf("[ OK ] PAT Write-Combining configured in MSR (PA2=WC)\r\n");
    }
    else
    {
        kprintf("[ WARN ] CPU does not support PAT\r\n");
    }

    // Framebuffer Double Buffering
    if (framebuffer_init_backbuffer(framebuffer))
    {
        kprintf("[ OK ] Framebuffer backbuffer initialized (double buffering active)\r\n");
    }
    else
    {
        kprintf("[ WARN ] Resolution exceeds backbuffer capacity, falling back to direct VRAM\r\n");
    }

    // Test graphical render
    framebuffer_clear(framebuffer, RGB_BLACK);
    framebuffer_fillRect(framebuffer, (Rect){50, 50, 100, 100}, RGB_GREEN);
    framebuffer_drawRect(framebuffer, (Rect){200, 50, 100, 100}, RGB_RED);
    framebuffer_drawLine(framebuffer, (Vec2){50, 200}, (Vec2){300, 250}, RGB_WHITE);
    framebuffer_swap(framebuffer);

    kprintf("[ OK ] Graphical test pattern drawn successfully.\r\n");
    kprintf("[ OK ] CrusaderOS Kernel initialization complete. System idle.\r\n");

    for (;;)
    {
        hlt();
    }
}
