#ifndef SYS_GDT_H_INCLUDED
#define SYS_GDT_H_INCLUDED

#include <stdint.h>

// GDT Selectors (SYSCALL / SYSRET compatible layout)
#define GDT_KERNEL_CODE_SELECTOR 0x08
#define GDT_KERNEL_DATA_SELECTOR 0x10
#define GDT_USER_CODE32_SELECTOR 0x18
#define GDT_USER_DATA_SELECTOR   0x20
#define GDT_USER_CODE_SELECTOR   0x28
#define GDT_TSS_SELECTOR         0x30

void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);

#endif // SYS_GDT_H_INCLUDED