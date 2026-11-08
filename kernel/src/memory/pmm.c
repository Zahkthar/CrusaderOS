#include "memory/pmm.h"

#include <stdbool.h>

#include "klibc/stdio.h"
#include "klibc/string.h"
#include "sys/cpu.h"

// +--------------------+
// | INTERNAL VARIABLES |
// +--------------------+

static uint8_t *s_bitmap = NULL;
static size_t s_total_pages = 0;
static size_t s_free_pages = 0;
static size_t s_used_pages = 0;
static size_t s_bitmap_size = 0;
static size_t s_last_allocated_index = 0;
static uint64_t s_hhdm_offset = 0;

// +------------------+
// | BITMAP UTILITIES |
// +------------------+

static inline void bitmap_set(size_t page_idx)
{
    s_bitmap[page_idx / 8] |= (uint8_t)(1 << (page_idx % 8));
}

static inline void bitmap_clear(size_t page_idx)
{
    s_bitmap[page_idx / 8] &= (uint8_t)~(1 << (page_idx % 8));
}

static inline bool bitmap_test(size_t page_idx)
{
    return (s_bitmap[page_idx / 8] & (1 << (page_idx % 8))) != 0;
}

// +-----------+
// | FUNCTIONS |
// +-----------+

void pmm_init(const BootInfo *boot_info)
{
    if (boot_info == NULL || boot_info->memmap == NULL)
    {
        kprintf("[ FATAL ] PMM: Invalid boot info or missing memory map!\r\n");
        hcf();
    }

    s_hhdm_offset = boot_info->hhdm_offset;
    struct limine_memmap_response *memmap = boot_info->memmap;

    // Déterminer l'adresse physique la plus haute pour dimensionner le bitmap
    uint64_t highest_address = 0;
    for (size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE)
        {
            uint64_t top = entry->base + entry->length;
            if (top > highest_address)
            {
                highest_address = top;
            }
        }
    }

    s_total_pages = highest_address / PAGE_SIZE;
    s_bitmap_size = (s_total_pages + 7) / 8;
    size_t bitmap_page_count = (s_bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Trouver une région physique assez pour le bitmap
    uint64_t bitmap_phys_addr = 0;
    for (size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE && entry->base >= 0x100000 &&
            entry->length >= (bitmap_page_count * PAGE_SIZE))
        {
            bitmap_phys_addr = entry->base;
            break;
        }
    }

    // Si aucune région >= 1 MiB n'est trouvée, chercher dans n'importe quelle entrée exploitable
    if (bitmap_phys_addr == 0)
    {
        for (size_t i = 0; i < memmap->entry_count; i++)
        {
            struct limine_memmap_entry *entry = memmap->entries[i];
            if (entry->type == LIMINE_MEMMAP_USABLE && entry->length >= (bitmap_page_count * PAGE_SIZE))
            {
                bitmap_phys_addr = entry->base;
                break;
            }
        }
    }

    if (bitmap_phys_addr == 0)
    {
        kprintf("[ FATAL ] PMM: Not enough contiguous memory to place the bitmap!\r\n");
        hcf();
    }

    // Accès virtuel au bitmap via le Higher Half Direct Map (HHDM)
    s_bitmap = (uint8_t *)(bitmap_phys_addr + s_hhdm_offset);

    // Initialiser le bitmap en marquant toutes les pages comme occupées (fail-safe)
    memset(s_bitmap, 0xFF, s_bitmap_size);
    s_free_pages = 0;

    // Marquer comme libres les pages appartenant aux régions LIMINE_MEMMAP_USABLE
    for (size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        if (entry->type == LIMINE_MEMMAP_USABLE)
        {
            uint64_t start_page = (entry->base + PAGE_SIZE - 1) / PAGE_SIZE;
            uint64_t end_page = (entry->base + entry->length) / PAGE_SIZE;

            if (end_page > s_total_pages)
            {
                end_page = s_total_pages;
            }

            for (uint64_t p = start_page; p < end_page; p++)
            {
                bitmap_clear(p);
                s_free_pages++;
            }
        }
    }

    // Marquer occupé les pages occupées par le bitmap lui-même
    uint64_t bitmap_start_page = bitmap_phys_addr / PAGE_SIZE;
    for (size_t i = 0; i < bitmap_page_count; i++)
    {
        bitmap_set(bitmap_start_page + i);
        s_free_pages--;
    }

    // Marquer occupé la première page (0x0000 - 0x0FFF, IVT/BDA) pour éviter l'allocation de la page NULL
    if (!bitmap_test(0))
    {
        bitmap_set(0);
        s_free_pages--;
    }

    s_used_pages = s_total_pages - s_free_pages;
    s_last_allocated_index = 0;

    kprintf("[ OK ] PMM initialized: %zu MiB total RAM (%zu pages), %zu MiB free (%zu pages)\r\n",
            (s_total_pages * PAGE_SIZE) / (1024 * 1024), s_total_pages, (s_free_pages * PAGE_SIZE) / (1024 * 1024),
            s_free_pages);
    kprintf("[ INFO ] PMM bitmap placed at phys %p (size %zu bytes, %zu pages)\r\n", (void *)bitmap_phys_addr,
            s_bitmap_size, bitmap_page_count);
}

void *pmm_alloc_page(void)
{
    if (s_free_pages == 0)
    {
        return NULL;
    }

    for (size_t i = 0; i < s_total_pages; i++)
    {
        size_t idx = (s_last_allocated_index + i) % s_total_pages;

        // Saut rapide, si l'octet vaut 0xFF, les 8 pages sont déjà occupées
        if (s_bitmap[idx / 8] == 0xFF)
        {
            size_t remainder = idx % 8;
            i += (7 - remainder);
            continue;
        }

        if (!bitmap_test(idx))
        {
            bitmap_set(idx);
            s_free_pages--;
            s_used_pages++;
            s_last_allocated_index = idx + 1;
            return (void *)(uintptr_t)(idx * PAGE_SIZE);
        }
    }

    return NULL;
}

void *pmm_alloc_pages(size_t count)
{
    if (count == 0 || s_free_pages < count)
    {
        return NULL;
    }

    if (count == 1)
    {
        return pmm_alloc_page();
    }

    size_t consecutive = 0;
    size_t start_idx = 0;

    for (size_t i = 0; i < s_total_pages; i++)
    {
        if (!bitmap_test(i))
        {
            if (consecutive == 0)
            {
                start_idx = i;
            }
            consecutive++;

            if (consecutive == count)
            {
                for (size_t j = 0; j < count; j++)
                {
                    bitmap_set(start_idx + j);
                }
                s_free_pages -= count;
                s_used_pages += count;
                s_last_allocated_index = start_idx + count;
                return (void *)(uintptr_t)(start_idx * PAGE_SIZE);
            }
        }
        else
        {
            consecutive = 0;
        }
    }

    return NULL;
}

void pmm_free_page(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }

    uintptr_t addr = (uintptr_t)ptr;
    size_t idx = addr / PAGE_SIZE;

    if (idx >= s_total_pages)
    {
        return;
    }

    if (bitmap_test(idx))
    {
        bitmap_clear(idx);
        s_free_pages++;
        s_used_pages--;
        if (idx < s_last_allocated_index)
        {
            s_last_allocated_index = idx;
        }
    }
}

void pmm_free_pages(void *ptr, size_t count)
{
    if (ptr == NULL || count == 0)
    {
        return;
    }

    uintptr_t addr = (uintptr_t)ptr;
    for (size_t i = 0; i < count; i++)
    {
        pmm_free_page((void *)(addr + i * PAGE_SIZE));
    }
}

size_t pmm_get_total_memory(void)
{
    return s_total_pages * PAGE_SIZE;
}

size_t pmm_get_free_memory(void)
{
    return s_free_pages * PAGE_SIZE;
}

size_t pmm_get_used_memory(void)
{
    return s_used_pages * PAGE_SIZE;
}

size_t pmm_get_total_pages(void)
{
    return s_total_pages;
}

size_t pmm_get_free_pages(void)
{
    return s_free_pages;
}

size_t pmm_get_used_pages(void)
{
    return s_used_pages;
}
