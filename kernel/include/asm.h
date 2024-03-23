#ifndef KERNEL_ASM_H_INCLUDED
#define KERNEL_ASM_H_INCLUDED

// +----------------------+
// | INTERRUPTS AND PAUSE |
// +----------------------+

void cli(void);
void sti(void);

void hlt(void);
void hcf(void);

#endif // KERNEL_ASM_H_INCLUDED