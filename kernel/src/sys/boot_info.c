#include "sys/boot_info.h"

#include <stddef.h>

// +-----------------+
// | LIMINE REQUESTS |
// +-----------------+

__attribute__((used)) static volatile LIMINE_BASE_REVISION(1);

__attribute__((used)) static volatile struct limine_framebuffer_request s_fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0
};

__attribute__((used)) static volatile struct limine_hhdm_request s_hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0
};

__attribute__((used)) static volatile struct limine_memmap_request s_mm_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0
};

__attribute__((used)) static volatile struct limine_rsdp_request s_rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .revision = 0
};

__attribute__((used)) static volatile struct limine_kernel_address_request s_kernel_addr_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0
};

__attribute__((used)) static volatile struct limine_module_request s_module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .revision = 0
};

// +--------------------+
// | INTERNAL VARIABLES |
// +--------------------+

static BootInfo s_boot_info;
static bool s_boot_info_initialized = false;

// +-----------+
// | FUNCTIONS |
// +-----------+

bool boot_info_init(void)
{
    if (s_boot_info_initialized)
    {
        return true;
    }

    if (LIMINE_BASE_REVISION_SUPPORTED == false)
    {
        return false;
    }

    if (s_hhdm_request.response == NULL || s_mm_request.response == NULL || s_kernel_addr_request.response == NULL)
    {
        return false;
    }

    if (s_fb_request.response == NULL || s_fb_request.response->framebuffer_count < 1)
    {
        return false;
    }

    s_boot_info.hhdm_offset = s_hhdm_request.response->offset;
    s_boot_info.memmap = s_mm_request.response;
    s_boot_info.framebuffer = s_fb_request.response->framebuffers[0];
    s_boot_info.rsdp_address = s_rsdp_request.response ? s_rsdp_request.response->address : NULL;
    s_boot_info.kernel_phys_base = s_kernel_addr_request.response->physical_base;
    s_boot_info.kernel_virt_base = s_kernel_addr_request.response->virtual_base;
    s_boot_info.modules = s_module_request.response;

    s_boot_info_initialized = true;
    return true;
}

const BootInfo *boot_info_get(void)
{
    if (!s_boot_info_initialized)
    {
        if (!boot_info_init())
        {
            return NULL;
        }
    }
    return &s_boot_info;
}
