#include "memory/heap.h"

#include <stdbool.h>

#include "klibc/stdio.h"
#include "klibc/string.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "sys/cpu.h"

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

#define HEAP_MAGIC          0xCAFEBABE12345678ULL
#define HEAP_MIN_SPLIT_SIZE 32

// +------------+
// | STRUCTURES |
// +------------+

typedef struct HeapBlock
{
    uint64_t magic;
    size_t size;
    uint64_t is_free;
    struct HeapBlock *next;
    struct HeapBlock *prev;
} __attribute__((aligned(16))) HeapBlock;

// +--------------------+
// | INTERNAL VARIABLES |
// +--------------------+

static HeapBlock *s_first_block = NULL;
static uint64_t s_heap_start = KERNEL_HEAP_START;
static uint64_t s_heap_end = KERNEL_HEAP_START;
static size_t s_heap_total_size = 0;
static size_t s_heap_used_size = 0;

// +------------------+
// | HELPER FUNCTIONS |
// +------------------+

static inline size_t align_up(size_t value, size_t alignment)
{
    return (value + alignment - 1) & ~(alignment - 1);
}

static bool heap_expand(size_t min_bytes)
{
    size_t required_bytes = min_bytes + sizeof(HeapBlock);
    size_t required_pages = (required_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    if (required_pages < 4)
    {
        required_pages = 4;
    }

    uint64_t new_pages_vaddr = s_heap_end;

    for (size_t i = 0; i < required_pages; i++)
    {
        void *phys = pmm_alloc_page();
        if (phys == NULL)
        {
            kprintf("[ ERROR ] Heap: PMM out of memory while expanding heap!\r\n");
            return false;
        }

        if (!vmm_map_page(vmm_get_kernel_directory(), s_heap_end, (uint64_t)phys,
                          PTE_PRESENT | PTE_WRITABLE | PTE_NO_EXECUTE))
        {
            pmm_free_page(phys);
            kprintf("[ ERROR ] Heap: VMM mapping failed while expanding heap!\r\n");
            return false;
        }

        s_heap_end += PAGE_SIZE;
        s_heap_total_size += PAGE_SIZE;
    }

    HeapBlock *new_block = (HeapBlock *)new_pages_vaddr;
    new_block->magic = HEAP_MAGIC;
    new_block->size = (required_pages * PAGE_SIZE) - sizeof(HeapBlock);
    new_block->is_free = 1;
    new_block->next = NULL;
    new_block->prev = NULL;

    if (s_first_block == NULL)
    {
        s_first_block = new_block;
    }
    else
    {
        HeapBlock *curr = s_first_block;
        while (curr->next != NULL)
        {
            curr = curr->next;
        }
        curr->next = new_block;
        new_block->prev = curr;

        // Fusionner avec le bloc précédent s'il est libre
        if (curr->is_free)
        {
            curr->size += sizeof(HeapBlock) + new_block->size;
            curr->next = NULL;
        }
    }

    return true;
}

static void split_block(HeapBlock *block, size_t size)
{
    if (block->size >= size + sizeof(HeapBlock) + HEAP_MIN_SPLIT_SIZE)
    {
        HeapBlock *remainder = (HeapBlock *)((uint8_t *)block + sizeof(HeapBlock) + size);
        remainder->magic = HEAP_MAGIC;
        remainder->size = block->size - size - sizeof(HeapBlock);
        remainder->is_free = 1;
        remainder->next = block->next;
        remainder->prev = block;

        if (block->next != NULL)
        {
            block->next->prev = remainder;
        }
        block->next = remainder;
        block->size = size;
    }
}

static void coalesce_block(HeapBlock *block)
{
    // Fusionner avec le bloc suivant
    if (block->next != NULL && block->next->is_free)
    {
        block->size += sizeof(HeapBlock) + block->next->size;
        block->next = block->next->next;
        if (block->next != NULL)
        {
            block->next->prev = block;
        }
    }

    // Fusionner avec le bloc précédent
    if (block->prev != NULL && block->prev->is_free)
    {
        block->prev->size += sizeof(HeapBlock) + block->size;
        block->prev->next = block->next;
        if (block->next != NULL)
        {
            block->next->prev = block->prev;
        }
    }
}

// +-----------+
// | FUNCTIONS |
// +-----------+

void heap_init(void)
{
    s_heap_start = KERNEL_HEAP_START;
    s_heap_end = KERNEL_HEAP_START;
    s_heap_total_size = 0;
    s_heap_used_size = 0;
    s_first_block = NULL;

    if (!heap_expand(KERNEL_HEAP_INITIAL_PAGES * PAGE_SIZE))
    {
        kprintf("[ FATAL ] Failed to initialize kernel heap!\r\n");
        hcf();
    }

    kprintf("[ OK ] Kernel heap initialized at virtual %p (initial size %zu KiB)\r\n", (void *)s_heap_start,
            s_heap_total_size / 1024);
}

void *kmalloc(size_t size)
{
    if (size == 0)
    {
        return NULL;
    }

    size = align_up(size, KERNEL_HEAP_ALIGNMENT);

    HeapBlock *curr = s_first_block;
    while (curr != NULL)
    {
        if (curr->magic != HEAP_MAGIC)
        {
            kprintf("[ FATAL ] Heap corruption detected in kmalloc! (magic: 0x%016lx)\r\n", curr->magic);
            hcf();
        }

        if (curr->is_free && curr->size >= size)
        {
            split_block(curr, size);
            curr->is_free = 0;
            s_heap_used_size += curr->size;
            return (void *)((uint8_t *)curr + sizeof(HeapBlock));
        }

        curr = curr->next;
    }

    // Aucun bloc libre suffisant : étendre le tas
    if (!heap_expand(size))
    {
        return NULL;
    }

    // Nouvelle recherche après extension
    curr = s_first_block;
    while (curr != NULL)
    {
        if (curr->is_free && curr->size >= size)
        {
            split_block(curr, size);
            curr->is_free = 0;
            s_heap_used_size += curr->size;
            return (void *)((uint8_t *)curr + sizeof(HeapBlock));
        }
        curr = curr->next;
    }

    return NULL;
}

void kfree(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    HeapBlock *block = (HeapBlock *)((uint8_t *)ptr - sizeof(HeapBlock));

    if (block->magic != HEAP_MAGIC)
    {
        kprintf("[ FATAL ] Heap corruption or invalid pointer in kfree! (magic: 0x%016lx)\r\n", block->magic);
        hcf();
    }

    if (block->is_free)
    {
        kprintf("[ WARN ] Double free detected on pointer %p!\r\n", ptr);
        return;
    }

    block->is_free = 1;
    s_heap_used_size -= block->size;
    coalesce_block(block);
}

void *kcalloc(size_t num, size_t size)
{
    if (num != 0 && size > (SIZE_MAX / num))
    {
        return NULL;
    }

    size_t total = num * size;
    void *ptr = kmalloc(total);
    if (ptr != NULL)
    {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *krealloc(void *ptr, size_t new_size)
{
    if (ptr == NULL)
    {
        return kmalloc(new_size);
    }

    if (new_size == 0)
    {
        kfree(ptr);
        return NULL;
    }

    HeapBlock *block = (HeapBlock *)((uint8_t *)ptr - sizeof(HeapBlock));
    if (block->magic != HEAP_MAGIC)
    {
        kprintf("[ FATAL ] Heap corruption in krealloc! (magic: 0x%016lx)\r\n", block->magic);
        hcf();
    }

    new_size = align_up(new_size, KERNEL_HEAP_ALIGNMENT);

    if (block->size >= new_size)
    {
        split_block(block, new_size);
        return ptr;
    }

    void *new_ptr = kmalloc(new_size);
    if (new_ptr == NULL)
    {
        return NULL;
    }

    memcpy(new_ptr, ptr, block->size);
    kfree(ptr);
    return new_ptr;
}

size_t heap_get_total_size(void)
{
    return s_heap_total_size;
}

size_t heap_get_used_size(void)
{
    return s_heap_used_size;
}

size_t heap_get_free_size(void)
{
    return s_heap_total_size > s_heap_used_size ? (s_heap_total_size - s_heap_used_size) : 0;
}
