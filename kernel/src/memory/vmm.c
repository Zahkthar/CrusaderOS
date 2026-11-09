#include "memory/vmm.h"

#include <stdbool.h>

#include "klibc/stdio.h"
#include "klibc/string.h"
#include "memory/pmm.h"
#include "sys/cpu.h"
#include "sys/spinlock.h"

// +----------------+
// | LINKER SYMBOLS |
// +----------------+

extern uint8_t __text_start[];
extern uint8_t __text_end[];
extern uint8_t __rodata_start[];
extern uint8_t __rodata_end[];
extern uint8_t __data_start[];
extern uint8_t __bss_end[];

// +--------------------+
// | INTERNAL VARIABLES |
// +--------------------+

static Spinlock s_vmm_lock = SPINLOCK_INIT;

#define MAX_PAGE_DIRECTORIES 16

static PageDirectory s_kernel_dir;
static PageDirectory s_dir_pool[MAX_PAGE_DIRECTORIES];
static bool s_dir_pool_used[MAX_PAGE_DIRECTORIES] = {false};

static uint64_t s_hhdm_offset = 0;
static bool s_vmm_initialized = false;

// +------------------+
// | HELPER FUNCTIONS |
// +------------------+

static void vmm_enable_nxe(void)
{
    uint32_t eax, ebx, ecx, edx;
    cpuid(0x80000001, &eax, &ebx, &ecx, &edx);
    if (edx & CPUID_EDX_XD)
    {
        uint64_t efer = rdmsr(MSR_EFER);
        if (!(efer & (1ULL << 11)))
        {
            wrmsr(MSR_EFER, efer | (1ULL << 11));
        }
    }
}

static uint64_t *vmm_get_next_level(uint64_t *current_table, size_t index, bool allocate, uint64_t flags)
{
    if (current_table[index] & PTE_PRESENT)
    {
        if (flags & PTE_USER)
        {
            current_table[index] |= PTE_USER;
        }
        uintptr_t phys = current_table[index] & PTE_ADDR_MASK;
        return (uint64_t *)(phys + s_hhdm_offset);
    }

    if (!allocate)
    {
        return NULL;
    }

    void *new_table_phys = pmm_alloc_page();
    if (new_table_phys == NULL)
    {
        return NULL;
    }

    uint64_t *new_table_virt = (uint64_t *)((uintptr_t)new_table_phys + s_hhdm_offset);
    memset(new_table_virt, 0, PAGE_SIZE);

    uint64_t entry_flags = PTE_PRESENT | PTE_WRITABLE;
    if (flags & PTE_USER)
    {
        entry_flags |= PTE_USER;
    }

    current_table[index] = (uintptr_t)new_table_phys | entry_flags;
    return new_table_virt;
}

// +-----------+
// | FUNCTIONS |
// +-----------+

void vmm_invlpg(uint64_t vaddr)
{
    asm volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
}

void vmm_switch_directory(PageDirectory *dir)
{
    if (dir == NULL || dir->cr3 == 0)
    {
        return;
    }

    write_cr3(dir->cr3);
}

PageDirectory *vmm_get_kernel_directory(void)
{
    return &s_kernel_dir;
}

static uint64_t *vmm_get_pte(PageDirectory *dir, uint64_t vaddr, bool allocate, uint64_t flags)
{
    if (dir == NULL || dir->entries == NULL)
    {
        return NULL;
    }

    size_t pml4_idx = VMM_PML4_INDEX(vaddr);
    size_t pdpt_idx = VMM_PDPT_INDEX(vaddr);
    size_t pd_idx = VMM_PD_INDEX(vaddr);
    size_t pt_idx = VMM_PT_INDEX(vaddr);

    uint64_t *pdpt = vmm_get_next_level(dir->entries, pml4_idx, allocate, flags);
    if (pdpt == NULL)
    {
        return NULL;
    }

    uint64_t *pd = vmm_get_next_level(pdpt, pdpt_idx, allocate, flags);
    if (pd == NULL)
    {
        return NULL;
    }

    uint64_t *pt = vmm_get_next_level(pd, pd_idx, allocate, flags);
    if (pt == NULL)
    {
        return NULL;
    }

    return &pt[pt_idx];
}

bool vmm_map_page(PageDirectory *dir, uint64_t vaddr, uint64_t paddr, uint64_t flags)
{
    vaddr &= ~0xFFFULL;
    paddr &= ~0xFFFULL;

    uint64_t irq_flags = spinlock_acquire_irqsave(&s_vmm_lock);

    uint64_t *pte = vmm_get_pte(dir, vaddr, true, flags);
    if (pte == NULL)
    {
        spinlock_release_irqrestore(&s_vmm_lock, irq_flags);
        return false;
    }

    *pte = paddr | flags;

    if (dir->cr3 == read_cr3())
    {
        vmm_invlpg(vaddr);
    }

    spinlock_release_irqrestore(&s_vmm_lock, irq_flags);
    return true;
}

bool vmm_unmap_page(PageDirectory *dir, uint64_t vaddr)
{
    vaddr &= ~0xFFFULL;

    uint64_t irq_flags = spinlock_acquire_irqsave(&s_vmm_lock);

    uint64_t *pte = vmm_get_pte(dir, vaddr, false, 0);
    if (pte == NULL)
    {
        spinlock_release_irqrestore(&s_vmm_lock, irq_flags);
        return false;
    }

    *pte = 0;

    if (dir->cr3 == read_cr3())
    {
        vmm_invlpg(vaddr);
    }

    spinlock_release_irqrestore(&s_vmm_lock, irq_flags);
    return true;
}

bool vmm_map_pages(PageDirectory *dir, uint64_t vaddr, uint64_t paddr, size_t count, uint64_t flags)
{
    for (size_t i = 0; i < count; i++)
    {
        if (!vmm_map_page(dir, vaddr + (i * PAGE_SIZE), paddr + (i * PAGE_SIZE), flags))
        {
            return false;
        }
    }
    return true;
}

bool vmm_unmap_pages(PageDirectory *dir, uint64_t vaddr, size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (!vmm_unmap_page(dir, vaddr + (i * PAGE_SIZE)))
        {
            return false;
        }
    }
    return true;
}

uint64_t vmm_virt_to_phys(PageDirectory *dir, uint64_t vaddr)
{
    uint64_t irq_flags = spinlock_acquire_irqsave(&s_vmm_lock);

    uint64_t *pte = vmm_get_pte(dir, vaddr, false, 0);
    if (pte == NULL || !(*pte & PTE_PRESENT))
    {
        spinlock_release_irqrestore(&s_vmm_lock, irq_flags);
        return 0;
    }

    uint64_t paddr = (*pte & PTE_ADDR_MASK) | VMM_PAGE_OFFSET(vaddr);
    spinlock_release_irqrestore(&s_vmm_lock, irq_flags);
    return paddr;
}

PageDirectory *vmm_create_directory(void)
{
    int slot = -1;
    for (int i = 0; i < MAX_PAGE_DIRECTORIES; i++)
    {
        if (!s_dir_pool_used[i])
        {
            slot = i;
            break;
        }
    }

    if (slot == -1)
    {
        return NULL;
    }

    void *pml4_phys = pmm_alloc_page();
    if (pml4_phys == NULL)
    {
        return NULL;
    }

    uint64_t *pml4_virt = (uint64_t *)((uintptr_t)pml4_phys + s_hhdm_offset);
    memset(pml4_virt, 0, PAGE_SIZE);

    // Partager les entrées de la moitié supérieure (espace noyau, entrées 256 à 511)
    for (size_t i = 256; i < 512; i++)
    {
        pml4_virt[i] = s_kernel_dir.entries[i];
    }

    s_dir_pool_used[slot] = true;
    s_dir_pool[slot].entries = pml4_virt;
    s_dir_pool[slot].cr3 = (uintptr_t)pml4_phys;

    return &s_dir_pool[slot];
}

void vmm_destroy_directory(PageDirectory *dir)
{
    if (dir == NULL || dir == &s_kernel_dir || dir->entries == NULL)
    {
        return;
    }

    // Libérer l'arborescence des tables utilisateur (entrées 0 à 255)
    for (size_t pml4_idx = 0; pml4_idx < 256; pml4_idx++)
    {
        if (!(dir->entries[pml4_idx] & PTE_PRESENT))
        {
            continue;
        }

        uint64_t *pdpt = (uint64_t *)((dir->entries[pml4_idx] & PTE_ADDR_MASK) + s_hhdm_offset);
        for (size_t pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++)
        {
            if (!(pdpt[pdpt_idx] & PTE_PRESENT))
            {
                continue;
            }

            uint64_t *pd = (uint64_t *)((pdpt[pdpt_idx] & PTE_ADDR_MASK) + s_hhdm_offset);
            for (size_t pd_idx = 0; pd_idx < 512; pd_idx++)
            {
                if (!(pd[pd_idx] & PTE_PRESENT))
                {
                    continue;
                }

                pmm_free_page((void *)(uintptr_t)(pd[pd_idx] & PTE_ADDR_MASK));
            }
            pmm_free_page((void *)(uintptr_t)(pdpt[pdpt_idx] & PTE_ADDR_MASK));
        }
        pmm_free_page((void *)(uintptr_t)(dir->entries[pml4_idx] & PTE_ADDR_MASK));
    }

    pmm_free_page((void *)dir->cr3);

    for (int i = 0; i < MAX_PAGE_DIRECTORIES; i++)
    {
        if (&s_dir_pool[i] == dir)
        {
            s_dir_pool_used[i] = false;
            break;
        }
    }
}

void vmm_init(const BootInfo *boot_info)
{
    if (s_vmm_initialized || boot_info == NULL || boot_info->memmap == NULL)
    {
        return;
    }

    s_hhdm_offset = boot_info->hhdm_offset;

    // Activer le bit No-Execute (NX) dans le MSR EFER
    vmm_enable_nxe();

    // Allouer la table PML4 du noyau
    void *kernel_pml4_phys = pmm_alloc_page();
    if (kernel_pml4_phys == NULL)
    {
        kprintf("[ FATAL ] VMM: Failed to allocate kernel PML4!\r\n");
        hcf();
    }

    s_kernel_dir.cr3 = (uintptr_t)kernel_pml4_phys;
    s_kernel_dir.entries = (uint64_t *)((uintptr_t)kernel_pml4_phys + s_hhdm_offset);
    memset(s_kernel_dir.entries, 0, PAGE_SIZE);

    // Mapper les sections du noyau avec leurs privilèges
    uint64_t text_start = (uint64_t)__text_start;
    uint64_t text_end = (uint64_t)__text_end;
    size_t text_pages = (text_end - text_start + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t text_phys = boot_info->kernel_phys_base + (text_start - boot_info->kernel_virt_base);
    vmm_map_pages(&s_kernel_dir, text_start, text_phys, text_pages, PTE_PRESENT);

    uint64_t rodata_start = (uint64_t)__rodata_start;
    uint64_t rodata_end = (uint64_t)__rodata_end;
    size_t rodata_pages = (rodata_end - rodata_start + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t rodata_phys = boot_info->kernel_phys_base + (rodata_start - boot_info->kernel_virt_base);
    vmm_map_pages(&s_kernel_dir, rodata_start, rodata_phys, rodata_pages, PTE_PRESENT | PTE_NO_EXECUTE);

    uint64_t data_start = (uint64_t)__data_start;
    uint64_t data_end = (uint64_t)__bss_end;
    size_t data_pages = (data_end - data_start + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t data_phys = boot_info->kernel_phys_base + (data_start - boot_info->kernel_virt_base);
    vmm_map_pages(&s_kernel_dir, data_start, data_phys, data_pages, PTE_PRESENT | PTE_WRITABLE | PTE_NO_EXECUTE);

    // Mapper toutes les régions physiques dans le Higher Half Direct Map (HHDM)
    struct limine_memmap_response *memmap = boot_info->memmap;
    for (size_t i = 0; i < memmap->entry_count; i++)
    {
        struct limine_memmap_entry *entry = memmap->entries[i];
        uint64_t start_phys = entry->base & ~0xFFFULL;
        uint64_t end_phys = (entry->base + entry->length + PAGE_SIZE - 1) & ~0xFFFULL;
        size_t pages = (end_phys - start_phys) / PAGE_SIZE;

        uint64_t flags = PTE_PRESENT | PTE_WRITABLE | PTE_NO_EXECUTE;
        if (entry->type == LIMINE_MEMMAP_FRAMEBUFFER)
        {
            // PCD=1, PWT=0 sélectionne PA2 (Write-Combining) dans le MSR PAT
            flags |= PTE_CACHE_DISABLE;
        }

        vmm_map_pages(&s_kernel_dir, start_phys + s_hhdm_offset, start_phys, pages, flags);
    }

    // Garantir que le framebuffer est correctement mappé avec Write-Combining
    if (boot_info->framebuffer != NULL && boot_info->framebuffer->address != NULL)
    {
        uint64_t fb_vaddr = (uint64_t)boot_info->framebuffer->address;
        uint64_t fb_size = (uint64_t)boot_info->framebuffer->pitch * boot_info->framebuffer->height;
        uint64_t fb_paddr = fb_vaddr - s_hhdm_offset;
        size_t fb_pages = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;

        vmm_map_pages(&s_kernel_dir, fb_vaddr, fb_paddr, fb_pages,
                      PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE | PTE_NO_EXECUTE);
    }

    // Activer le nouveau répertoire de pagination du noyau
    vmm_switch_directory(&s_kernel_dir);
    s_vmm_initialized = true;

    kprintf("[ OK ] VMM initialized: CR3 loaded with new kernel PML4 (phys %p)\r\n", (void *)s_kernel_dir.cr3);
    kprintf("[ OK ] Kernel memory protected (.text=RX, .rodata=R, .data/.bss=RW, NX active)\r\n");
}
