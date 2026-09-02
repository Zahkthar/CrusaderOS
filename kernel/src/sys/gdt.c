#include "sys/gdt.h"

#include <stdint.h>

typedef struct GDTEntry
{
    uint16_t limitLow;
    uint16_t baseLow;
    uint8_t baseMiddle;
    uint8_t accessByte;
    uint8_t limitHighFlags; // limitHigh = & 0x0F; flags = & 0xF0
    uint8_t baseHigh;
} __attribute__((packed)) GDTEntry;

typedef struct GDTDescriptor
{
    uint16_t size;
    GDTEntry *entries;
} __attribute__((packed)) GDTDescriptor;

typedef enum GDTAccessByte
{
    GDT_ACCESS_CODE_READABLE = 0x02,
    GDT_ACCESS_DATA_WRITEABLE = 0x02,

    GDT_ACCESS_CODE_CONFORMING = 0x04,
    GDT_ACCESS_DATA_DIRECTION_NORMAL = 0x00,
    GDT_ACCESS_DATA_DIRECTION_DOWN = 0x04,

    GDT_ACCESS_DATA_SEGMENT = 0x10,
    GDT_ACCESS_CODE_SEGMENT = 0x18,

    GDT_ACCESS_DESCRIPTOR_TSS = 0x00,

    GDT_ACCESS_RING0 = 0x00,
    GDT_ACCESS_RING1 = 0x20,
    GDT_ACCESS_RING2 = 0x40,
    GDT_ACCESS_RING3 = 0x60,

    GDT_ACCESS_PRESENT = 0x80
} GDTAccessByte;

typedef enum
{
    GDT_FLAG_64BIT = 0x20,
    GDT_FLAG_32BIT = 0x40,
    GDT_FLAG_16BIT = 0x00,

    GDT_FLAG_GRANULARITY_1B = 0x00,
    GDT_FLAG_GRANULARITY_4K = 0x80
} GDT_FLAGS;

// Helper macros
#define GDT_LIMIT_LOW(limit)             (limit & 0xFFFF)
#define GDT_BASE_LOW(base)               (base & 0xFFFF)
#define GDT_BASE_MIDDLE(base)            ((base >> 16) & 0xFF)
#define GDT_FLAGS_LIMIT_HI(limit, flags) (((limit >> 16) & 0xF) | (flags & 0xF0))
#define GDT_BASE_HIGH(base)              ((base >> 24) & 0xFF)

#define GDT_ENTRY(base, limit, access, flags)                                                                          \
    {GDT_LIMIT_LOW(limit), GDT_BASE_LOW(base), GDT_BASE_MIDDLE(base), access, GDT_FLAGS_LIMIT_HI(limit, flags),        \
     GDT_BASE_HIGH(base)}

static GDTEntry g_GDT[] = {
    // NULL descriptor
    GDT_ENTRY(0, 0, 0, 0),

    // Kernel mode code segment
    GDT_ENTRY(0, 0xFFFFF, (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_CODE_SEGMENT | GDT_ACCESS_CODE_READABLE),
              (GDT_FLAG_64BIT | GDT_FLAG_GRANULARITY_4K)),

    // Kernel mode data segment
    GDT_ENTRY(0, 0xFFFFF, (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DATA_SEGMENT | GDT_ACCESS_DATA_WRITEABLE),
              (GDT_FLAG_32BIT | GDT_FLAG_GRANULARITY_4K)),

    // User mode code segment
    GDT_ENTRY(0, 0xFFFFF, (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_CODE_SEGMENT | GDT_ACCESS_CODE_READABLE),
              (GDT_FLAG_64BIT | GDT_FLAG_GRANULARITY_4K)),

    // User mode data segment
    GDT_ENTRY(0, 0xFFFFF, (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DATA_SEGMENT | GDT_ACCESS_DATA_WRITEABLE),
              (GDT_FLAG_32BIT | GDT_FLAG_GRANULARITY_4K))

    // TSS
};

static GDTDescriptor g_GDTDescriptor = {sizeof(g_GDT) - 1, g_GDT};

void gdt_init(void)
{
    // Charger la table GDT et recharger CS ainsi que les segments de données
    asm volatile("lgdt (%0)\n"
                 "push $0x08\n" // 0x08 = offset du segment de code noyau 64-bit
                 "lea 1f(%%rip), %%rax\n"
                 "push %%rax\n"
                 "lretq\n"           // Far return pour recharger CS
                 "1:\n"
                 "mov $0x10, %%ax\n" // 0x10 = offset du segment de données noyau
                 "mov %%ax, %%ds\n"
                 "mov %%ax, %%es\n"
                 "mov %%ax, %%fs\n"
                 "mov %%ax, %%gs\n"
                 "mov %%ax, %%ss\n"
                 :
                 : "r"(&g_GDTDescriptor)
                 : "rax", "memory");
}