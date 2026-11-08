#ifndef MEMORY_PMM_H_INCLUDED
#define MEMORY_PMM_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

#include "sys/boot_info.h"

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

#define PAGE_SIZE  4096
#define PAGE_SHIFT 12

// +-----------+
// | FUNCTIONS |
// +-----------+

void pmm_init(const BootInfo *boot_info);

void *pmm_alloc_page(void);
void *pmm_alloc_pages(size_t count);
void pmm_free_page(void *ptr);
void pmm_free_pages(void *ptr, size_t count);

size_t pmm_get_total_memory(void);
size_t pmm_get_free_memory(void);
size_t pmm_get_used_memory(void);

size_t pmm_get_total_pages(void);
size_t pmm_get_free_pages(void);
size_t pmm_get_used_pages(void);

#endif // MEMORY_PMM_H_INCLUDED
