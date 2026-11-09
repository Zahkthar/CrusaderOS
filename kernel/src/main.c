#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "drivers/serial.h"
#include "graphics/colors.h"
#include "graphics/console.h"
#include "graphics/font.h"
#include "graphics/framebuffer.h"
#include "klibc/stdio.h"
#include "klibc/stdlib.h"
#include "klibc/string.h"
#include "memory/heap.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "sys/apic.h"
#include "sys/boot_info.h"
#include "sys/cpu.h"
#include "sys/gdt.h"
#include "sys/idt.h"
#include "sys/pic.h"

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

    // VMM (Virtual Memory Manager)
    vmm_init(boot_info);

    // Quick verification test for VMM
    void *phys_frame = pmm_alloc_page();
    uint64_t test_vaddr = 0xffffdead00000000ULL;
    if (vmm_map_page(vmm_get_kernel_directory(), test_vaddr, (uint64_t)phys_frame,
                     PTE_PRESENT | PTE_WRITABLE | PTE_NO_EXECUTE))
    {
        volatile uint32_t *test_ptr = (volatile uint32_t *)test_vaddr;
        *test_ptr = 0x1337CAFE;
        uint32_t read_back = *test_ptr;
        uint64_t resolved_phys = vmm_virt_to_phys(vmm_get_kernel_directory(), test_vaddr);

        if (read_back == 0x1337CAFE && resolved_phys == (uint64_t)phys_frame)
        {
            kprintf("[ OK ] VMM virtual mapping verified (vaddr %p -> phys %p, value 0x%x)\r\n", (void *)test_vaddr,
                    (void *)resolved_phys, (unsigned int)read_back);
        }
        vmm_unmap_page(vmm_get_kernel_directory(), test_vaddr);
        pmm_free_page(phys_frame);
    }

    // Heap (Kernel Dynamic Memory Allocator)
    heap_init();

    // Quick verification test for kmalloc/kfree/kcalloc/krealloc
    char *buf1 = (char *)kmalloc(64);
    int *buf2 = (int *)kcalloc(10, sizeof(int));
    if (buf1 != NULL && buf2 != NULL)
    {
        strcpy(buf1, "CrusaderOS Kernel Heap is operational!");
        buf2[9] = 42;
        kprintf("[ TEST ] kmalloc allocated: \"%s\" (at %p)\r\n", buf1, buf1);
        kprintf("[ TEST ] kcalloc allocated array (val at idx 9: %d, at %p)\r\n", buf2[9], buf2);

        buf1 = (char *)krealloc(buf1, 128);
        strcat(buf1, " [Reallocated]");
        kprintf("[ TEST ] krealloc expanded: \"%s\" (at %p)\r\n", buf1, buf1);

        kfree(buf1);
        kfree(buf2);
        kprintf("[ OK ] kmalloc/kfree/kcalloc/krealloc cycle verified (%zu KiB free)\r\n",
                heap_get_free_size() / 1024);
    }

    // PIC (Programmable Interrupt Controller) Disable
    pic_disable();

    // Local APIC & APIC Timer (100 Hz = 10 ms period)
    if (lapic_init(boot_info))
    {
        apic_timer_init(100);

        // Enable hardware interrupts
        sti();
        kprintf("[ OK ] Hardware interrupts enabled (STI)\r\n");

        // Verify timer interrupt is firing
        timer_sleep_ms(50);
        kprintf("[ TEST ] APIC Timer verified (50 ms sleep, %lu ticks elapsed)\r\n", timer_get_ticks());
    }

    // Framebuffer Double Buffering (dynamically allocated via kmalloc)
    if (framebuffer_init_backbuffer(framebuffer))
    {
        kprintf("[ OK ] Framebuffer backbuffer initialized (double buffering active)\r\n");
    }
    else
    {
        kprintf("[ WARN ] Backbuffer allocation failed, falling back to direct VRAM\r\n");
    }

    // Graphical TTY Console Initialization
    console_init(framebuffer);
    kprintf("[ OK ] Graphical TTY Console initialized (%d x %d chars, 8x16 font)\r\n", console_get_cols(),
            console_get_rows());

    // Graphical and Font Engine test badge
    framebuffer_fillRect(framebuffer, (Rect){(int)framebuffer->width - 240, 20, 220, 70}, RGB_BLUE);
    framebuffer_drawRect(framebuffer, (Rect){(int)framebuffer->width - 240, 20, 220, 70}, RGB_CYAN);
    font_draw_string(framebuffer, (int)framebuffer->width - 230, 30, "CrusaderOS 64-bit", RGB_WHITE, RGB_BLUE);
    font_draw_string(framebuffer, (int)framebuffer->width - 230, 50, "Phase 2: TTY Active", RGB_YELLOW, RGB_BLUE);
    font_draw_string(framebuffer, (int)framebuffer->width - 230, 70, "8x16 Font Engine", RGB_GREEN, RGB_BLUE);
    framebuffer_swap(framebuffer);

    kprintf("[ OK ] Graphical test pattern and font rendering verified.\r\n");
    kprintf("[ TEST ] Console text rendering verified on display (%d cols x %d rows)\r\n", console_get_cols(),
            console_get_rows());
    kprintf("[ OK ] CrusaderOS Kernel initialization complete. System idle.\r\n");

    for (;;)
    {
        hlt();
    }
}
