#include "memory/heap.h"

#include <stdbool.h>

#include "klibc/stdio.h"
#include "klibc/string.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "sys/cpu.h"
#include "sys/spinlock.h"

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

static Spinlock s_heap_lock = SPINLOCK_INIT;

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

static void heap_rollback_pages(uint64_t start_vaddr, size_t count)
{
    for (size_t j = 0; j < count; j++)
    {
        uint64_t vaddr = start_vaddr + (j * PAGE_SIZE);
        uint64_t paddr = vmm_virt_to_phys(vmm_get_kernel_directory(), vaddr);
        vmm_unmap_page(vmm_get_kernel_directory(), vaddr);
        if (paddr != 0)
        {
            pmm_free_page((void *)(uintptr_t)paddr);
        }
    }
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
    size_t pages_allocated = 0;

    for (size_t i = 0; i < required_pages; i++)
    {
        void *phys = pmm_alloc_page();
        if (phys == NULL)
        {
            kprintf("[ ERROR ] Heap: PMM out of memory while expanding heap!\r\n");
            heap_rollback_pages(new_pages_vaddr, pages_allocated);
            s_heap_end = new_pages_vaddr;
            s_heap_total_size -= (pages_allocated * PAGE_SIZE);
            return false;
        }

        if (!vmm_map_page(vmm_get_kernel_directory(), s_heap_end, (uint64_t)phys,
                          PTE_PRESENT | PTE_WRITABLE | PTE_NO_EXECUTE))
        {
            pmm_free_page(phys);
            kprintf("[ ERROR ] Heap: VMM mapping failed while expanding heap!\r\n");
            heap_rollback_pages(new_pages_vaddr, pages_allocated);
            s_heap_end = new_pages_vaddr;
            s_heap_total_size -= (pages_allocated * PAGE_SIZE);
            return false;
        }

        s_heap_end += PAGE_SIZE;
        s_heap_total_size += PAGE_SIZE;
        pages_allocated++;
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

        // Fusionner le remainder avec le bloc suivant s'il est libre
        if (remainder->next != NULL && remainder->next->is_free)
        {
            remainder->size += sizeof(HeapBlock) + remainder->next->size;
            remainder->next = remainder->next->next;
            if (remainder->next != NULL)
            {
                remainder->next->prev = remainder;
            }
        }
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

    uint64_t flags = spinlock_acquire_irqsave(&s_heap_lock);

    HeapBlock *curr = s_first_block;
    while (curr != NULL)
    {
        if (curr->magic != HEAP_MAGIC)
        {
            spinlock_release_irqrestore(&s_heap_lock, flags);
            kprintf("[ FATAL ] Heap corruption detected in kmalloc! (magic: 0x%016lx)\r\n", curr->magic);
            hcf();
        }

        if (curr->is_free && curr->size >= size)
        {
            split_block(curr, size);
            curr->is_free = 0;
            s_heap_used_size += curr->size;
            spinlock_release_irqrestore(&s_heap_lock, flags);
            return (void *)((uint8_t *)curr + sizeof(HeapBlock));
        }

        curr = curr->next;
    }

    // Aucun bloc libre suffisant : étendre le tas
    if (!heap_expand(size))
    {
        spinlock_release_irqrestore(&s_heap_lock, flags);
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
            spinlock_release_irqrestore(&s_heap_lock, flags);
            return (void *)((uint8_t *)curr + sizeof(HeapBlock));
        }
        curr = curr->next;
    }

    spinlock_release_irqrestore(&s_heap_lock, flags);
    return NULL;
}

void kfree(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    uint64_t flags = spinlock_acquire_irqsave(&s_heap_lock);

    HeapBlock *block = (HeapBlock *)((uint8_t *)ptr - sizeof(HeapBlock));

    if (block->magic != HEAP_MAGIC)
    {
        spinlock_release_irqrestore(&s_heap_lock, flags);
        kprintf("[ FATAL ] Heap corruption or invalid pointer in kfree! (magic: 0x%016lx)\r\n", block->magic);
        hcf();
    }

    if (block->is_free)
    {
        spinlock_release_irqrestore(&s_heap_lock, flags);
        kprintf("[ WARN ] Double free detected on pointer %p!\r\n", ptr);
        return;
    }

    block->is_free = 1;
    if (s_heap_used_size >= block->size)
    {
        s_heap_used_size -= block->size;
    }
    else
    {
        s_heap_used_size = 0;
    }
    coalesce_block(block);

    spinlock_release_irqrestore(&s_heap_lock, flags);
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

    uint64_t flags = spinlock_acquire_irqsave(&s_heap_lock);
    if (block->size >= new_size)
    {
        size_t old_size = block->size;
        split_block(block, new_size);
        s_heap_used_size -= (old_size - block->size);
        spinlock_release_irqrestore(&s_heap_lock, flags);
        return ptr;
    }
    spinlock_release_irqrestore(&s_heap_lock, flags);

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
