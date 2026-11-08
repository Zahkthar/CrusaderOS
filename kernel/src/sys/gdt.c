#include "sys/gdt.h"

#include "klibc/string.h"

// Task State Segment (TSS) for x86_64
typedef struct TSS
{
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) TSS;

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
    uint64_t base;
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

    GDT_ACCESS_DESCRIPTOR_TSS = 0x09,

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
#define GDT_LIMIT_LOW(limit)             ((limit) & 0xFFFF)
#define GDT_BASE_LOW(base)               ((base) & 0xFFFF)
#define GDT_BASE_MIDDLE(base)            (((base) >> 16) & 0xFF)
#define GDT_FLAGS_LIMIT_HI(limit, flags) ((((limit) >> 16) & 0xF) | ((flags) & 0xF0))
#define GDT_BASE_HIGH(base)              (((base) >> 24) & 0xFF)

#define GDT_ENTRY(base, limit, access, flags)                                                                          \
    {GDT_LIMIT_LOW(limit), GDT_BASE_LOW(base), GDT_BASE_MIDDLE(base), access, GDT_FLAGS_LIMIT_HI(limit, flags),        \
     GDT_BASE_HIGH(base)}

static GDTEntry g_GDT[8] = {
    // 0x00: NULL descriptor
    GDT_ENTRY(0, 0, 0, 0),

    // 0x08: Kernel mode code segment (Ring 0, 64-bit)
    GDT_ENTRY(0, 0xFFFFF, (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_CODE_SEGMENT | GDT_ACCESS_CODE_READABLE),
              (GDT_FLAG_64BIT | GDT_FLAG_GRANULARITY_4K)),

    // 0x10: Kernel mode data segment (Ring 0)
    GDT_ENTRY(0, 0xFFFFF, (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DATA_SEGMENT | GDT_ACCESS_DATA_WRITEABLE),
              (GDT_FLAG_32BIT | GDT_FLAG_GRANULARITY_4K)),

    // 0x18: User mode code segment 32-bit (Ring 3, compatibility mode required by SYSRET)
    GDT_ENTRY(0, 0xFFFFF, (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_CODE_SEGMENT | GDT_ACCESS_CODE_READABLE),
              (GDT_FLAG_32BIT | GDT_FLAG_GRANULARITY_4K)),

    // 0x20: User mode data segment (Ring 3)
    GDT_ENTRY(0, 0xFFFFF, (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_DATA_SEGMENT | GDT_ACCESS_DATA_WRITEABLE),
              (GDT_FLAG_32BIT | GDT_FLAG_GRANULARITY_4K)),

    // 0x28: User mode code segment 64-bit (Ring 3)
    GDT_ENTRY(0, 0xFFFFF, (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | GDT_ACCESS_CODE_SEGMENT | GDT_ACCESS_CODE_READABLE),
              (GDT_FLAG_64BIT | GDT_FLAG_GRANULARITY_4K)),

    // 0x30 & 0x38: TSS (16 octets / 2 entrées GDT, initialisé dynamiquement)
    GDT_ENTRY(0, 0, 0, 0),
    GDT_ENTRY(0, 0, 0, 0)
};

static TSS g_TSS;

// Dedicated IST stack for Double Fault (#DF) to prevent Triple Fault on kernel stack overflow
#define DF_STACK_SIZE 0x2000 // 8 KiB
static uint8_t s_df_stack[DF_STACK_SIZE] __attribute__((aligned(16)));

static void gdt_set_tss(int index, uintptr_t base, uint32_t limit)
{
    g_GDT[index].limitLow = (uint16_t)(limit & 0xFFFF);
    g_GDT[index].baseLow = (uint16_t)(base & 0xFFFF);
    g_GDT[index].baseMiddle = (uint8_t)((base >> 16) & 0xFF);
    g_GDT[index].accessByte = GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | GDT_ACCESS_DESCRIPTOR_TSS;
    g_GDT[index].limitHighFlags = (uint8_t)((limit >> 16) & 0x0F);
    g_GDT[index].baseHigh = (uint8_t)((base >> 24) & 0xFF);

    uint32_t *high_entry = (uint32_t *)&g_GDT[index + 1];
    high_entry[0] = (uint32_t)(base >> 32);
    high_entry[1] = 0;
}

void tss_set_rsp0(uint64_t rsp0)
{
    g_TSS.rsp0 = rsp0;
}

void tss_set_ist(int index, uint64_t ist)
{
    switch (index)
    {
    case 1:
        g_TSS.ist1 = ist;
        break;
    case 2:
        g_TSS.ist2 = ist;
        break;
    case 3:
        g_TSS.ist3 = ist;
        break;
    case 4:
        g_TSS.ist4 = ist;
        break;
    case 5:
        g_TSS.ist5 = ist;
        break;
    case 6:
        g_TSS.ist6 = ist;
        break;
    case 7:
        g_TSS.ist7 = ist;
        break;
    default:
        break;
    }
}

void gdt_init(void)
{
    // Initialisation du TSS
    memset(&g_TSS, 0, sizeof(g_TSS));
    g_TSS.iopb_offset = sizeof(g_TSS);

    // Initialisation de la pile de secours IST1 pour #DF (Double Fault)
    tss_set_ist(1, (uint64_t)s_df_stack + DF_STACK_SIZE);

    // Configuration de l'entrée TSS dans la GDT (sélecteur 0x30, index 6)
    gdt_set_tss(6, (uintptr_t)&g_TSS, sizeof(g_TSS) - 1);

    // Descripteur GDTR alloué localement sur la pile
    // Élimine toute relocalisation non alignée dans la section .data / .rela.dyn
    volatile GDTDescriptor gdtr = {
        .size = sizeof(g_GDT) - 1,
        .base = (uint64_t)&g_GDT
    };

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
                 "mov %%ax, %%ss\n"
                 :
                 : "r"(&gdtr)
                 : "rax", "memory");

    // Charger le sélecteur TSS dans le Task Register (TR)
    asm volatile("ltr %%ax" : : "a"((uint16_t)GDT_TSS_SELECTOR));
}