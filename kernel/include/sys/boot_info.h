#ifndef SYS_BOOT_INFO_H_INCLUDED
#define SYS_BOOT_INFO_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <limine.h>

// +------------+
// | STRUCTURES |
// +------------+

typedef struct BootInfo
{
    uint64_t hhdm_offset;
    struct limine_memmap_response *memmap;
    struct limine_framebuffer *framebuffer;
    void *rsdp_address;
    uint64_t kernel_phys_base;
    uint64_t kernel_virt_base;
    struct limine_module_response *modules;
} BootInfo;

// +-----------+
// | FUNCTIONS |
// +-----------+

bool boot_info_init(void);
const BootInfo *boot_info_get(void);

#endif // SYS_BOOT_INFO_H_INCLUDED
