#ifndef SYS_GDT_H_INCLUDED
#define SYS_GDT_H_INCLUDED

#include <stdint.h>

// GDT Selectors (SYSCALL / SYSRET compatible layout)
// Hardware mapping via IA32_STAR (MSR 0xC0000081):
// - SYSCALL:          STAR[47:32] = 0x08 -> Kernel CS = 0x08, Kernel SS = 0x10
// - SYSRETQ (64-bit): STAR[63:48] = 0x18 -> User SS = (0x18 + 8) | 3 = 0x23, User CS = (0x18 + 16) | 3 = 0x2B
// - SYSRET (32-bit):  STAR[63:48] = 0x18 -> User CS = 0x18 | 3 = 0x1B, User SS = (0x18 + 8) | 3 = 0x23
#define GDT_KERNEL_CODE_SELECTOR 0x08
#define GDT_KERNEL_DATA_SELECTOR 0x10
#define GDT_USER_CODE32_SELECTOR 0x18
#define GDT_USER_DATA_SELECTOR   0x20
#define GDT_USER_CODE_SELECTOR   0x28
#define GDT_TSS_SELECTOR         0x30

void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);
void tss_set_ist(int index, uint64_t ist);

#endif // SYS_GDT_H_INCLUDED