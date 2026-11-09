#include "sys/apic.h"

#include "klibc/stdio.h"
#include "memory/vmm.h"
#include "sys/cpu.h"
#include "sys/idt.h"

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

#define MSR_APIC_BASE    0x1B
#define APIC_BASE_ENABLE (1ULL << 11)

// Local APIC Register Offsets
#define LAPIC_REG_ID               0x0020
#define LAPIC_REG_VERSION          0x0030
#define LAPIC_REG_TPR              0x0080
#define LAPIC_REG_EOI              0x00B0
#define LAPIC_REG_LDR              0x00D0
#define LAPIC_REG_DFR              0x00E0
#define LAPIC_REG_SVR              0x00F0
#define LAPIC_REG_ESR              0x0280
#define LAPIC_REG_LVT_TIMER        0x0320
#define LAPIC_REG_LVT_LINT0        0x0350
#define LAPIC_REG_LVT_LINT1        0x0360
#define LAPIC_REG_LVT_ERROR        0x0370
#define LAPIC_REG_TIMER_INIT_COUNT 0x0380
#define LAPIC_REG_TIMER_CURR_COUNT 0x0390
#define LAPIC_REG_TIMER_DIV_CONFIG 0x03E0

#define LAPIC_TIMER_MODE_PERIODIC (1 << 17)
#define LAPIC_TIMER_MASKED        (1 << 16)
#define LAPIC_SVR_SOFTWARE_ENABLE (1 << 8)

// PIT Ports for timer calibration
#define PIT_CHANNEL2_DATA 0x42
#define PIT_COMMAND_PORT  0x43
#define PIT_SYS_CONTROL_B 0x61

// +--------------------+
// | INTERNAL VARIABLES |
// +--------------------+

static uintptr_t s_lapic_virt = 0;
static volatile uint64_t s_timer_ticks = 0;
static uint32_t s_ticks_per_10ms = 0;
static bool s_lapic_initialized = false;

// +------------------+
// | HELPER FUNCTIONS |
// +------------------+

static inline void lapic_write(uint32_t reg, uint32_t value)
{
    *(volatile uint32_t *)(s_lapic_virt + reg) = value;
}

static inline uint32_t lapic_read(uint32_t reg)
{
    return *(volatile uint32_t *)(s_lapic_virt + reg);
}

static void apic_timer_handler(InterruptRegisters *regs)
{
    (void)regs;
    s_timer_ticks++;
    lapic_send_eoi();
}

// +-----------+
// | FUNCTIONS |
// +-----------+

bool lapic_init(const BootInfo *boot_info)
{
    if (s_lapic_initialized)
    {
        return true;
    }

    if (boot_info == NULL)
    {
        return false;
    }

    // Verify APIC support via CPUID
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    if ((edx & CPUID_EDX_APIC) == 0)
    {
        kprintf("[ FATAL ] CPU does not support Local APIC!\r\n");
        return false;
    }

    // Ensure APIC is globally enabled in MSR
    uint64_t apic_base_msr = rdmsr(MSR_APIC_BASE);
    if ((apic_base_msr & APIC_BASE_ENABLE) == 0)
    {
        wrmsr(MSR_APIC_BASE, apic_base_msr | APIC_BASE_ENABLE);
        apic_base_msr = rdmsr(MSR_APIC_BASE);
    }

    uint64_t lapic_phys = apic_base_msr & 0xFFFFF000ULL;
    s_lapic_virt = lapic_phys + boot_info->hhdm_offset;

    // Ensure the Local APIC page is mapped in VMM
    vmm_map_page(vmm_get_kernel_directory(), s_lapic_virt, lapic_phys,
                 PTE_PRESENT | PTE_WRITABLE | PTE_CACHE_DISABLE | PTE_NO_EXECUTE);

    // Clear Task Priority Register to accept all interrupts
    lapic_write(LAPIC_REG_TPR, 0);

    // Set flat model destination format
    lapic_write(LAPIC_REG_DFR, 0xFFFFFFFF);
    lapic_write(LAPIC_REG_LDR, (lapic_read(LAPIC_REG_LDR) & 0x00FFFFFF) | 1);

    // Enable APIC by setting bit 8 in Spurious Interrupt Vector Register
    lapic_write(LAPIC_REG_SVR, LAPIC_SVR_SOFTWARE_ENABLE | APIC_SPURIOUS_VECTOR);

    // Configure LINT0 as ExtINT (0x0700) to route legacy PIC interrupts (Virtual Wire Mode)
    lapic_write(LAPIC_REG_LVT_LINT0, 0x0700);
    lapic_write(LAPIC_REG_LVT_LINT1, 0x10000);

    // Clear error register
    lapic_write(LAPIC_REG_ESR, 0);

    // Send End of Interrupt to clear any pending interrupt
    lapic_send_eoi();

    s_lapic_initialized = true;

    kprintf("[ OK ] Local APIC initialized (ID: %u, phys: %p)\r\n", lapic_get_id(), (void *)lapic_phys);
    return true;
}

void lapic_send_eoi(void)
{
    if (s_lapic_virt != 0)
    {
        lapic_write(LAPIC_REG_EOI, 0);
    }
}

uint32_t lapic_get_id(void)
{
    if (s_lapic_virt == 0)
    {
        return 0;
    }
    return (lapic_read(LAPIC_REG_ID) >> 24) & 0xFF;
}

void apic_timer_init(uint32_t frequency_hz)
{
    if (!s_lapic_initialized)
    {
        return;
    }

    // Register timer handler in IDT
    idt_register_handler(APIC_TIMER_VECTOR, apic_timer_handler);

    // Step 1: Calibrate APIC timer using PIT Channel 2 over a 10 ms interval
    // PIT oscillator frequency: 1193182 Hz. 10 ms = 11932 PIT cycles.
    uint8_t prev_control = inb(PIT_SYS_CONTROL_B);
    outb(PIT_SYS_CONTROL_B, (prev_control & ~0x02) | 0x01); // Gate 2 high, speaker low

    // Configure PIT Channel 2 for one-shot mode (Mode 0, binary, lobyte/hibyte)
    outb(PIT_COMMAND_PORT, 0xB0);
    outb(PIT_CHANNEL2_DATA, 11932 & 0xFF);
    outb(PIT_CHANNEL2_DATA, (11932 >> 8) & 0xFF);

    // Reset Gate 2 to start counter
    uint8_t val = inb(PIT_SYS_CONTROL_B) & 0xFE;
    outb(PIT_SYS_CONTROL_B, val);
    outb(PIT_SYS_CONTROL_B, val | 0x01);

    // Start LAPIC timer with divide by 16 and max count
    lapic_write(LAPIC_REG_TIMER_DIV_CONFIG, 0x03); // Divide by 16
    lapic_write(LAPIC_REG_TIMER_INIT_COUNT, 0xFFFFFFFF);

    // Wait until PIT counter reaches zero (bit 5 of port 0x61 goes high)
    while ((inb(PIT_SYS_CONTROL_B) & 0x20) == 0)
    {
    }

    // Stop APIC timer by masking
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MASKED);

    // Read elapsed ticks during 10 ms
    uint32_t ticks_10ms = 0xFFFFFFFF - lapic_read(LAPIC_REG_TIMER_CURR_COUNT);
    if (ticks_10ms < 1000)
    {
        ticks_10ms = 100000; // Fallback sanity check for hypervisors
    }

    s_ticks_per_10ms = ticks_10ms;

    // Restore PIT System Control Port B
    outb(PIT_SYS_CONTROL_B, prev_control);

    // Step 2: Configure APIC Timer in Periodic Mode
    // At frequency_hz (e.g., 100 Hz = 10 ms period):
    uint32_t ticks_per_period = (s_ticks_per_10ms * 100) / (frequency_hz > 0 ? frequency_hz : 100);

    lapic_write(LAPIC_REG_TIMER_DIV_CONFIG, 0x03); // Divide by 16
    lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_TIMER_MODE_PERIODIC | APIC_TIMER_VECTOR);
    lapic_write(LAPIC_REG_TIMER_INIT_COUNT, ticks_per_period);

    kprintf("[ OK ] APIC Timer initialized (Periodic %u Hz, %u ticks/period)\r\n", frequency_hz, ticks_per_period);
}

uint64_t timer_get_ticks(void)
{
    return s_timer_ticks;
}

void timer_sleep_ms(uint64_t ms)
{
    // At 100 Hz, 1 tick = 10 ms
    uint64_t target_ticks = s_timer_ticks + (ms + 9) / 10;
    while (s_timer_ticks < target_ticks)
    {
        hlt();
    }
}
