#ifndef MEMORY_VMM_H_INCLUDED
#define MEMORY_VMM_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sys/boot_info.h"

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

#define PTE_PRESENT       (1ULL << 0)
#define PTE_WRITABLE      (1ULL << 1)
#define PTE_USER          (1ULL << 2)
#define PTE_WRITE_THROUGH (1ULL << 3)
#define PTE_CACHE_DISABLE (1ULL << 4)
#define PTE_ACCESSED      (1ULL << 5)
#define PTE_DIRTY         (1ULL << 6)
#define PTE_PAT           (1ULL << 7)
#define PTE_GLOBAL        (1ULL << 8)
#define PTE_NO_EXECUTE    (1ULL << 63)

#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define VMM_PML4_INDEX(vaddr)  (((vaddr) >> 39) & 0x1FF)
#define VMM_PDPT_INDEX(vaddr)  (((vaddr) >> 30) & 0x1FF)
#define VMM_PD_INDEX(vaddr)    (((vaddr) >> 21) & 0x1FF)
#define VMM_PT_INDEX(vaddr)    (((vaddr) >> 12) & 0x1FF)
#define VMM_PAGE_OFFSET(vaddr) ((vaddr) & 0xFFF)

// +------------+
// | STRUCTURES |
// +------------+

typedef struct PageDirectory
{
    uint64_t *entries;
    uintptr_t cr3;
} PageDirectory;

// +-----------+
// | FUNCTIONS |
// +-----------+

void vmm_init(const BootInfo *boot_info);

PageDirectory *vmm_get_kernel_directory(void);
PageDirectory *vmm_create_directory(void);
void vmm_destroy_directory(PageDirectory *dir);

bool vmm_map_page(PageDirectory *dir, uint64_t vaddr, uint64_t paddr, uint64_t flags);
bool vmm_unmap_page(PageDirectory *dir, uint64_t vaddr);

bool vmm_map_pages(PageDirectory *dir, uint64_t vaddr, uint64_t paddr, size_t count, uint64_t flags);
bool vmm_unmap_pages(PageDirectory *dir, uint64_t vaddr, size_t count);

uint64_t vmm_virt_to_phys(PageDirectory *dir, uint64_t vaddr);

void vmm_switch_directory(PageDirectory *dir);
void vmm_invlpg(uint64_t vaddr);

#endif // MEMORY_VMM_H_INCLUDED
