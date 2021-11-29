global start
extern long_mode_start

section .text
bits 32
start:
    mov esp, stack_top ; initialize the stack, we have 64 bytes space

    ; checks begin
    call check_multiboot
    call check_cpuid
    call check_long_mode
    ; checks end

    call set_up_page_tables
    call enable_paging

    ; load the 64-bit GDT
    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start

; According to the Multiboot specification,
; the bootloader must write the magic value 0x36d76289 to it before loading a kernel.
check_multiboot:
    cmp eax, 0x36d76289
    jne .no_multiboot
    ret
.no_multiboot:
    mov al, "0"
    jmp error

check_cpuid:
    ; Check if CPUID is supported by attempting to flip the ID bit (bit 21)
    ; in the FLAGS register. If we can flip it, CPUID is available.

    ; Copy FLAGS in to EAX via stack
    pushfd
    pop eax

    ; Copy to ECX as well for comparing later on
    mov ecx, eax

    ; Flip the ID bit
    xor eax, 1 << 21

    ; Copy EAX to FLAGS via the stack
    push eax
    popfd

    ; Copy FLAGS back to EAX (with the flipped bit if CPUID is supported)
    pushfd
    pop eax

    ; Restore FLAGS from the old version stored in ECX (i.e. flipping the
    ; ID bit back if it was ever flipped).
    push ecx
    popfd

    ; Compare EAX and ECX. If they are equal then that means the bit
    ; wasn't flipped, and CPUID isn't supported.
    cmp eax, ecx
    je .no_cpuid
    ret
.no_cpuid:
    mov al, "1"
    jmp error


check_long_mode:
    ; test if extended processor info in available
    mov eax, 0x80000000    ; implicit argument for cpuid
    cpuid                  ; get highest supported argument
    cmp eax, 0x80000001    ; it needs to be at least 0x80000001
    jb .no_long_mode       ; if it's less, the CPU is too old for long mode

    ; use extended info to test if long mode is available
    mov eax, 0x80000001    ; argument for extended processor info
    cpuid                  ; returns various feature bits in ecx and edx
    test edx, 1 << 29      ; test if the LM-bit is set in the D-register
    jz .no_long_mode       ; If it's not set, there is no long mode
    ret
.no_long_mode:
    mov al, "2"
    jmp error


; Prints `ERR: ` and the given error code to screen and hangs.
; parameter: error code (in ascii) in al
error:
    mov dword [0xb8000], 0x4f524f45 ; 'ER'
    mov dword [0xb8004], 0x4f3a4f52 ; 'R:'
    mov dword [0xb8008], 0x4f204f20; '  '
    mov byte  [0xb800a], al
    hlt

; Paging
; In long mode, x86 uses a page size of 4096 bytes and a 4 level page table that consists of:

;     the Page-Map Level-4 Table (PML4),
;     the Page-Directory Pointer Table (PDP),
;     the Page-Directory Table (PD),
;     and the Page Table (PT).
; Each page table contains 512 entries and one entry is 8 bytes,
; so they fit exactly in one page (512*8 = 4096)
;
; Convert a virtual address to physical address
;       Get the address of the PML4 table from the CR3 register
;       Use bits 39-47 (9 bits) as an index into PML4 (2^9 = 512 = number of entries)
;       Use the following 9 bits as an index into PDP
;       Use the following 9 bits as an index into PD
;       Use the following 9 bits as an index into PT
;       Use the last 12 bits as page offset (2^12 = 4096 = page size)
;       The bits 48-63 must be copies of bit 47, so each valid virtual address is still unique.
;    .---------------------- nominal address ----------------------.
;    |                                                             |
;    +--------16|-------9|-------9|-------9|-------9|------------12|
;    | sign ext | p4 idx | p3 idx | p2 idx | p1 idx | frame offset |
;    63-------48|47----39|38----30|29----21|20----12|11------------0
;               |                                   |              |
;               '-------- effective address --------'              |
;               |                                                  |
;               '---------------- logical address -----------------'

; An entry in the P4, P3, P2, and P1 tables consists of
; the page aligned 52-bit physical address of the frame or the next page table
; and the following bits that can be OR-ed in:
; Bit(s)    Name                    Meaning
; 0         present                 the page is currently in memory
; 1         writable                it's allowed to write to this page
; 2         user accessible         if not set, only kernel mode code can access this page
; 3         write through caching   writes go directly to memory
; 4         disable cache           no cache is used for this page
; 5         accessed                the CPU sets this bit when this page is used
; 6         dirty                   the CPU sets this bit when a write to this page occurs
; 7         huge page/null          must be 0 in P1 and P4, creates a 1GiB page in P3,
;                                   creates a 2MiB page in P2
; 8         global                  page isn't flushed from caches on address space switch
;                                   (PGE bit of CR4 register must be set)
; 9-11      available               can be used freely by the OS
; 52-62     vailable                can be used freely by the OS
; 63        no execute              forbid executing code on this page
;                                   (the NXE bit in the EFER register must be set)
;    .---------------------- nominal address ----------------------.
;    |                                                             |
;    +---------1|----11|--------------------------40|-----3|------9|
;    | exec bit | open | page-aligned frame address | open | flags |
;    63-------63|62--52|51------------------------12|11---9|8------0
;                      |                            |              |
;                      '------ next page table -----'              |
;                      |                                           |
;                  OR: '----- logical address (+frame offset) -----'
set_up_page_tables:
    ; map first P4 entry to P3 table
    mov eax, p3_table
    or eax, 0b11 ; present + writable
    mov [p4_table], eax

    ; map first P3 entry to P2 table
    mov eax, p2_table
    or eax, 0b11 ; present + writable
    mov [p3_table], eax

    ; map each P2 entry to a huge 2MiB page
    mov ecx, 0         ; counter variable

.map_p2_table:
    ; map ecx-th P2 entry to a huge page that starts at address 2MiB*ecx
    mov eax, 0x200000  ; 2MiB
    mul ecx            ; start address of ecx-th page
    or eax, 0b10000011 ; present + writable + huge
    mov [p2_table + ecx * 8], eax ; map ecx-th entry

    inc ecx            ; increase counter
    cmp ecx, 512       ; if counter == 512, the whole P2 table is mapped
    jne .map_p2_table  ; else map the next entry

    ret

enable_paging:
    ; load P4 to cr3 register (cpu uses this to access the P4 table)
    mov eax, p4_table
    mov cr3, eax

    ; enable PAE-flag in cr4 (Physical Address Extension)
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; set the long mode bit in the EFER MSR (model specific register)
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; enable paging in the cr0 register
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

; Today almost everyone uses Paging instead of Segmentation (and so do we).
; But on x86, a GDT is always required, even when you're not using Segmentation.
; GRUB has set up a valid 32-bit GDT for us but now we need to switch to a long mode GDT.
; Bit(s)        Name            Meaning
; 0-41          ignored         ignored in 64-bit mode
; 42            conforming      the current privilege level can be higher than
;                               the specified level for code segments (else it must match exactly)
; 43            executable      if set, it's a code segment, else it's a data segment
; 44            descriptor type should be 1 for code and data segments
; 45-46         privilege       the ring level: 0 for kernel, 3 for user
; 47            present         must be 1 for valid selectors
; 48-52         ignored         ignored in 64-bit mode
; 53            64-bit          should be set for 64-bit code segments
; 54            32-bit          must be 0 for 64-bit segments
; 55-63         ignored         ignored in 64-bit mode
section .rodata
gdt64:
    dq 0 ; zero entry
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; code segment
.code: equ $ - gdt64
    dq (1<<43) | (1<<44) | (1<<47) | (1<<53) ; code segment
.pointer:
    dw $ - gdt64 - 1    ; number of GDT
    dq gdt64            ; start of GDT

section .bss
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096
stack_bottom:
    resb 64 ; reserve byte)
stack_top:
