#ifndef SYS_APIC_H_INCLUDED
#define SYS_APIC_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "sys/boot_info.h"
#include "sys/idt.h"

// +--------------------+
// | DEFINES AND MACROS |
// +--------------------+

#define APIC_TIMER_VECTOR    0x20
#define APIC_SPURIOUS_VECTOR 0xFF

// +-----------+
// | FUNCTIONS |
// +-----------+

bool lapic_init(const BootInfo *boot_info);
void lapic_send_eoi(void);
uint32_t lapic_get_id(void);

void apic_timer_init(uint32_t frequency_hz);
uint64_t timer_get_ticks(void);
void timer_sleep_ms(uint64_t ms);

#endif // SYS_APIC_H_INCLUDED
