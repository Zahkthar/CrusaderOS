#ifndef SYS_GDT_H_INCLUDED
#define SYS_GDT_H_INCLUDED

void gdt_init(void);
void tss_set_rsp0(uint64_t rsp0);

#endif // SYS_GDT_H_INCLUDED