#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "klibc/stdlib.h"

#include <limine.h>

#include "sys/cpu.h"
#include "sys/gdt.h"

#include "drivers/serial.h"

#include "graphics/colors.h"
#include "graphics/framebuffer.h"

// Limine requests

__attribute__((used)) static volatile LIMINE_BASE_REVISION(1);

__attribute__((used)) static volatile struct limine_framebuffer_request fb_request = {.id = LIMINE_FRAMEBUFFER_REQUEST,
                                                                                      .revision = 0};

__attribute__((used)) static volatile struct limine_hhdm_request hhdm_request = {.id = LIMINE_HHDM_REQUEST,
                                                                                 .revision = 0};

__attribute__((used)) static volatile struct limine_memmap_request mm_request = {.id = LIMINE_MEMMAP_REQUEST,
                                                                                 .revision = 0};

__attribute__((used)) static volatile struct limine_rsdp_request rsdp_request = {.id = LIMINE_RSDP_REQUEST,
                                                                                 .revision = 0};

__attribute__((used)) static volatile struct limine_kernel_address_request kernel_addr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST, .revision = 0};

__attribute__((used)) static volatile struct limine_module_request module_request = {.id = LIMINE_MODULE_REQUEST,
                                                                                     .revision = 0};

void kernel_main(void);

void kernel_main(void)
{
    if (LIMINE_BASE_REVISION_SUPPORTED == false)
    {
        hcf();
    }

    // Assure that we have a framebuffer
    if (fb_request.response == NULL || fb_request.response->framebuffer_count < 1)
    {
        hcf();
    }

    struct limine_framebuffer *framebuffer = fb_request.response->framebuffers[0];

    // Initializations

    // Serial communications (non-fatal if absent on real hardware)
    if (serial_init(SERIAL_COM1) == 0)
    {
        serial_write(SERIAL_COM1, "[ OK ] Serial Initialization\r\n");

        char strBuffer[100] = "";
        serial_write(SERIAL_COM1, "[ INFO ] Screen resolution : ");
        serial_write(SERIAL_COM1, itoa((int)framebuffer->width, strBuffer, 10));
        serial_write(SERIAL_COM1, "x");
        serial_write(SERIAL_COM1, itoa((int)framebuffer->height, strBuffer, 10));
        serial_write(SERIAL_COM1, "\r\n");
    }

    // GDT - Global Descriptor Table
    gdt_init();

    // IDT - Interrupt Descriptor Table

    // PMM - Physical Memory Manager

    // VMM - Virtual Memory Manager

    // Paging

    framebuffer_fillRect(framebuffer, (Rect){50, 50, 100, 100}, RGB_GREEN);
    framebuffer_drawRect(framebuffer, (Rect){200, 50, 100, 100}, RGB_RED);

    hcf(); // Done
}
