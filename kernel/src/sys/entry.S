.section .bss
.align 16
.global s_kernel_stack
.global s_kernel_stack_top

s_kernel_stack:
    .skip 0x10000 /* 64 KiB permanent kernel stack */
s_kernel_stack_top:

.section .text
.global kernel_main
.type kernel_main, @function
.extern kmain

kernel_main:
    /* Set up clean, permanent 16-byte aligned kernel stack */
    lea s_kernel_stack_top(%rip), %rsp
    xor %rbp, %rbp

    /* Call C kernel main entry */
    call kmain

    /* Safety halt loop if kmain ever returns */
    cli
1:  hlt
    jmp 1b
