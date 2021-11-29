#ifndef _GATE_INCLUDED
#define _GATE_INCLUDED

#include "type.h"

struct desc_struct {
    uint8 x[8];
};

struct gate_struct {
    uint8 x[16];
};

extern struct desc_struct GDT_Table[];
extern struct gate_struct IDT_Table[];
extern uint32 TSS64_Table[26];

#define _set_gate(gate_selector_addr, attr, ist, code_addr)                \
    do {                                                                   \
        uint64 __d0, __d1;                                                 \
        __asm__ __volatile__(                                              \
            "movw     %%dx,         %%ax    \n\t"                          \
            "andq     $0x7,         %%rcx   \n\t"                          \
            "addq     %4,           %%rcx   \n\t"                          \
            "shlq     $32,          %%rcx   \n\t"                          \
            "addq     %%rcx,        %%rax   \n\t"                          \
            "xorq     %%rcx,        %%rcx   \n\t"                          \
            "movl     %%edx,        %%ecx   \n\t"                          \
            "shrq     $16,          %%rcx   \n\t"                          \
            "shlq     $48,          %%rcx   \n\t"                          \
            "addq     %%rcx,        %%rax   \n\t"                          \
            "movq     %%rax,        %0      \n\t"                          \
            "shrq     $32,          %%rdx   \n\t"                          \
            "movq     %%rdx,        %1      \n\t"                          \
            : "=m"(*((uintptr *)(gate_selector_addr))),                    \
              "=m"(*(1 + (uintptr *)(gate_selector_addr))), "=&a"(__d0),   \
              "=&d"(__d1)                                                  \
            : "i"(attr << 8), "3"((uintptr *)(code_addr)), "2"(0x8 << 16), \
              "c"(ist)                                                     \
            : "memory");                                                   \
    } while (0)

#define load_TR(n)                                                   \
    do {                                                             \
        __asm__ __volatile__("ltr %%ax" : : "a"(n << 3) : "memory"); \
    } while (0)

static inline void set_intr_gate(uint32 n, uint8 ist, void *addr) {
    _set_gate(IDT_Table + n, 0x8E, ist, addr);  // P,DPL=0,TYPE=E
}

static inline void set_trap_gate(uint32 n, uint8 ist, void *addr) {
    _set_gate(IDT_Table + n, 0x8F, ist, addr);  // P,DPL=0,TYPE=F
}

static inline void set_system_gate(uint32 n, uint8 ist, void *addr) {
    _set_gate(IDT_Table + n, 0xEF, ist, addr);  // P,DPL=3,TYPE=F
}

static inline void set_system_intr_gate(uint32 n, uint8 ist,
                                        void *addr)  // int3
{
    _set_gate(IDT_Table + n, 0xEE, ist, addr);  // P,DPL=3,TYPE=E
}

static inline void set_tss64(uint64 rsp0, uint64 rsp1, uint64 rsp2, uint64 ist1,
                             uint64 ist2, uint64 ist3, uint64 ist4, uint64 ist5,
                             uint64 ist6, uint64 ist7) {
    *(uint64 *)(TSS64_Table + 1) = rsp0;
    *(uint64 *)(TSS64_Table + 3) = rsp1;
    *(uint64 *)(TSS64_Table + 5) = rsp2;

    *(uint64 *)(TSS64_Table + 9) = ist1;
    *(uint64 *)(TSS64_Table + 11) = ist2;
    *(uint64 *)(TSS64_Table + 13) = ist3;
    *(uint64 *)(TSS64_Table + 15) = ist4;
    *(uint64 *)(TSS64_Table + 17) = ist5;
    *(uint64 *)(TSS64_Table + 19) = ist6;
    *(uint64 *)(TSS64_Table + 21) = ist7;
}

#endif /* _GATE_INCLUDED */
