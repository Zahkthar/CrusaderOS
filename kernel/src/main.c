#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <limine.h>

#include "drivers/serial.h"
#include "graphics/colors.h"
#include "graphics/framebuffer.h"
#include "klibc/stdio.h"
#include "klibc/stdlib.h"
#include "sys/cpu.h"
#include "sys/gdt.h"
#include "sys/idt.h"

// Limine requests
__attribute__((used)) static volatile LIMINE_BASE_REVISION(1);

__attribute__((used)) static volatile struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used)) static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used)) static volatile struct limine_memmap_request mm_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used)) static volatile struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

__attribute__((used)) static volatile struct limine_kernel_address_request kernel_addr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0
};

__attribute__((used)) static volatile struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

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

    if (LIMINE_BASE_REVISION_SUPPORTED == false)
    {
        kprintf("[ FATAL ] Limine base revision unsupported!\r\n");
        hcf();
    }

    if (hhdm_request.response == NULL || mm_request.response == NULL || kernel_addr_request.response == NULL)
    {
        kprintf("[ FATAL ] Critical Limine boot responses missing!\r\n");
        hcf();
    }
    kprintf("[ OK ] Limine HHDM and Memmap verified\r\n");

    if (fb_request.response == NULL || fb_request.response->framebuffer_count < 1)
    {
        kprintf("[ FATAL ] No Limine framebuffer available!\r\n");
        hcf();
    }

    struct limine_framebuffer *framebuffer = fb_request.response->framebuffers[0];
    kprintf("[ INFO ] Framebuffer: %ux%u (%u bpp, pitch %u, addr %p)\r\n", (unsigned int)framebuffer->width,
            (unsigned int)framebuffer->height, (unsigned int)framebuffer->bpp, (unsigned int)framebuffer->pitch,
            framebuffer->address);

    // GDT (Global Descriptor Table) & TSS Init (Task State Segment)
    gdt_init();
    tss_set_rsp0((uint64_t)s_kernel_stack_top);
    kprintf("[ OK ] GDT and TSS configured (RSP0 set to %p)\r\n", s_kernel_stack_top);

    // IDT (Interrupt Descriptor Table)
    idt_init();

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
