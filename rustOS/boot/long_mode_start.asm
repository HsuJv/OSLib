global long_mode_start
;extern _start
section .text
bits 64
long_mode_start:
    ; load 0 into all data segment registers
    ; a few instructions that expect a valid data segment descriptor
    ; or the null descriptor in those registers.
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; call _start
    ; print `OKAY` to screen
    mov rax, 0x2f592f412f4b2f4f
    mov qword [0xb8000], rax
    hlt