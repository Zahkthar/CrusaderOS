#ifndef MEMORY_HEAP_H_INCLUDED
#define MEMORY_HEAP_H_INCLUDED

#include <stddef.h>
#include <stdint.h>

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

#define KERNEL_HEAP_START         0xFFFF900000000000ULL
#define KERNEL_HEAP_INITIAL_PAGES 16
#define KERNEL_HEAP_ALIGNMENT     16

// +-----------+
// | FUNCTIONS |
// +-----------+

void heap_init(void);

void *kmalloc(size_t size);
void kfree(void *ptr);
void *kcalloc(size_t num, size_t size);
void *krealloc(void *ptr, size_t new_size);

size_t heap_get_total_size(void);
size_t heap_get_used_size(void);
size_t heap_get_free_size(void);

#endif // MEMORY_HEAP_H_INCLUDED
