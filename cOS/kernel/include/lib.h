#ifndef _LIB_INCLUDED
#define _LIB_INCLUDED

#include "type.h"

/* macros */
#define container_of(ptr, type, member)                         \
    ({                                                          \
        typeof(((type *)0)->member) *p = (ptr);                 \
        (type *)((uint64)p - (uint64) & (((type *)0)->member)); \
    })

#define sti() __asm__ __volatile__("sti \n\t" ::: "memory")
#define cli() __asm__ __volatile__("cli \n\t" ::: "memory")
#define nop() __asm__ __volatile__("nop \n\t")
/*
 * to ensure the serialization of memory access,
 * the internal operation is to add a number of
 * delays in a series of memory accesses,
 * to ensure that the memory access after this
 * instruction occurs after the memory access
 * before this instruction (no overlap).
 *
 * lfence: read serialized
 * sfence: write serialized
 * mfence: read and write are serialized
 */
#define io_mfence() __asm__ __volatile__("mfence \n\t" ::: "memory")

/*
 * Use likely and unlike to expect branch prediction
 */
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 1)

/* structs */
struct List {
    struct List *prev;
    struct List *next;
};

/* inlines */

static inline void list_init(struct List *list) {
    list->prev = list;
    list->next = list;
}

static inline void list_add_to_behind(struct List *entry, struct List *new) {
    new->next = entry->next;
    new->prev = entry;
    new->next->prev = new;
    entry->next = new;
}

static inline void list_add_to_before(struct List *entry, struct List *new) {
    new->next = entry;
    entry->prev->next = new;
    new->prev = entry->prev;
    entry->prev = new;
}

static inline void list_del(struct List *entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
}

static inline bool list_is_empty(struct List *entry) {
    if (entry == entry->next && entry->prev == entry)
        return 1;
    else
        return 0;
}

static inline struct List *list_prev(struct List *entry) {
    if (entry->prev != NULL)
        return entry->prev;
    else
        return NULL;
}

static inline struct List *list_next(struct List *entry) {
    if (entry->next != NULL)
        return entry->next;
    else
        return NULL;
}

/*
                Src => Dst memory copy Num bytes
*/

static inline void *memcpy(void *Dst, const void *Src, const uint64 Num) {
    int d0, d1, d2;
    __asm__ __volatile__(
        "cld          \n\t"
        "rep movsq    \n\t"
        "testb $4,%b4 \n\t"
        "je 1f        \n\t"
        "movsl        \n\t"
        "1:           \n\t"
        "testb $2,%b4 \n\t"
        "je 2f        \n\t"
        "movsw        \n\t"
        "2:           \n\t"
        "testb $1,%b4 \n\t"
        "je 3f        \n\t"
        "movsb        \n\t"
        "3:           \n\t"
        : "=&c"(d0), "=&S"(d1), "=&D"(d2)
        : "0"(Num / 8), "q"(Num), "1"(Src), "2"(Dst)
        : "memory", "cc");
    return Dst;
}

/*
-                FirstPart = SecondPart         =>       0
-                FirstPart > SecondPart         =>       1
-                FirstPart < SecondPart         =>      -1
*/
static inline int memcmp(const void *FirstPart, const void *SecondPart,
                         const uint64 Count) {
    register int __res;

    __asm__ __volatile__(
        "cld            \n\t"
        "repe cmpsb     \n\t"
        "je 1f          \n\t"
        "movl $1, %%eax \n\t"
        "jl 1f          \n\t"
        "negl %%eax     \n\t"
        "1:             \n\t"
        : "=a"(__res)
        : "0"(0), "D"(FirstPart), "S"(SecondPart), "c"(Count)
        :);
    return __res;
}

/*
    set memory at Address with C, number is Count
*/

static inline void *memset(void *Address, const uint8 C, const uint64 Count) {
    int d0, d1;
    uint64 tmp = C * 0x0101010101010101UL;
    __asm__ __volatile__(
        "cld            \n\t"
        "rep stosq      \n\t"
        "testb $4, %b3  \n\t"
        "je 1f          \n\t"
        "stosl          \n\t"
        "1:             \n\t"
        "testb $2, %b3  \n\t"
        "je 2f          \n\t"
        "stosw          \n\t"
        "2:             \n\t"
        "testb $1, %b3  \n\t"
        "je 3f          \n\t"
        "stosb          \n\t"
        "3:             \n\t"
        : "=&c"(d0), "=&D"(d1)
        : "a"(tmp), "q"(Count), "0"(Count / 8), "1"(Address)
        : "memory");
    return Address;
}

/*
                string copy
*/

static inline char *strcpy(char *Dst, const char *Src) {
    int d0, d1, d2;
    __asm__ __volatile__(
        "cld            \n\t"
        "1:             \n\t"
        "lodsb          \n\t"
        "stosb          \n\t"
        "testb %%al,%%al\n\t"
        "jne 1b"
        : "=&S"(d0), "=&D"(d1), "=&a"(d2)
        : "0"(Src), "1"(Dst)
        : "memory");
    return Dst;
}

/*
                string copy number bytes
*/

static inline int8 *strncpy(int8 *Dest, const int8 *Src, const int64 Count) {
    __asm__ __volatile__(
        "cld                \n\t"
        "1:                 \n\t"
        "decq %2            \n\t"
        "js 2f              \n\t"
        "lodsb              \n\t"
        "stosb              \n\t"
        "testb %%al, %%al   \n\t"
        "jne 1b             \n\t"
        "rep stosb          \n\t"
        "2:                 \n\t"
        :
        : "S"(Src), "D"(Dest), "c"(Count)
        :);
    return Dest;
}

/*
                string cat Dest + Src
*/

static inline int8 *strcat(int8 *Dest, int8 *Src) {
    __asm__ __volatile__(
        "cld                \n\t"
        "repne scasb        \n\t"
        "decq %1            \n\t"
        "1:                 \n\t"
        "lodsb              \n\t"
        "stosb              \n\r"
        "testb %%al, %%al   \n\t"
        "jne 1b             \n\t"
        :
        : "S"(Src), "D"(Dest), "a"(0), "c"(0xffffffff)
        :);
    return Dest;
}

/*
                string compare FirstPart and SecondPart
                FirstPart = SecondPart =>  0
                FirstPart > SecondPart =>  1
                FirstPart < SecondPart => -1
*/

static inline int strcmp(const int8 *FirstPart, const int8 *SecondPart) {
    register int __res;
    __asm__ __volatile__(
        "cld                \n\t"
        "1:                 \n\t"
        "lodsb              \n\t"
        "scasb              \n\t"
        "jne 2f             \n\t"
        "testb %%al, %%al   \n\t"
        "jne 1b             \n\t"
        "xorl %%eax, %%eax  \n\t"
        "jmp 3f             \n\t"
        "2:                 \n\t"
        "movl $1, %%eax     \n\t"
        "jl 3f              \n\t"
        "negl %%eax         \n\t"
        "3:                 \n\t"
        : "=a"(__res)
        : "D"(FirstPart), "S"(SecondPart)
        :);
    return __res;
}

/*
                string compare FirstPart and SecondPart with Count Bytes
                FirstPart = SecondPart =>  0
                FirstPart > SecondPart =>  1
                FirstPart < SecondPart => -1
*/

static inline int strncmp(const int8 *FirstPart, const int8 *SecondPart,
                          const int64 Count) {
    register int __res;
    __asm__ __volatile__(
        "cld                \n\t"
        "1:                 \n\t"
        "decq %3            \n\t"
        "js 2f              \n\t"
        "lodsb              \n\t"
        "scasb              \n\t"
        "jne 3f             \n\t"
        "testb %%al, %%al   \n\t"
        "jne 1b             \n\t"
        "2:                 \n\t"
        "xorl %%eax, %%eax  \n\t"
        "jmp 4f             \n\t"
        "3:                 \n\t"
        "movl $1, %%eax     \n\t"
        "jl 4f              \n\t"
        "negl %%eax         \n\t"
        "4:                 \n\t"
        : "=a"(__res)
        : "D"(FirstPart), "S"(SecondPart), "c"(Count)
        :);
    return __res;
}

static inline int strlen(const int8 *String) {
    register int __res;
    __asm__ __volatile__(
        "cld        \n\t"
        "repne scasb\n\t"
        "notl %0    \n\t"
        "decl %0    \n\t"
        : "=c"(__res)
        : "D"(String), "a"(0), "0"(0xffffffff)
        :);
    return __res;
}

static inline uint64 bit_set(uint64 *addr, uint64 nr) {
    return *addr | (1UL << nr);
}

static inline uint64 bit_get(uint64 *addr, uint64 nr) {
    return *addr & (1UL << nr);
}

static inline uint64 bit_clean(uint64 *addr, uint64 nr) {
    return *addr & (~(1UL << nr));
}

static inline uint8 io_in8(uint16 port) {
    uint8 ret = 0;
    __asm__ __volatile__(
        "inb %%dx, %0   \n\t"
        "mfence         \n\t"
        : "=a"(ret)
        : "d"(port)
        : "memory");
    return ret;
}

static inline unsigned int io_in32(uint16 port) {
    unsigned int ret = 0;
    __asm__ __volatile__(
        "inl %%dx, %0   \n\t"
        "mfence         \n\t"
        : "=a"(ret)
        : "d"(port)
        : "memory");
    return ret;
}

static inline void io_out8(uint16 port, uint8 value) {
    __asm__ __volatile__(
        "outb %0, %%dx  \n\t"
        "mfence         \n\t"
        :
        : "a"(value), "d"(port)
        : "memory");
}

static inline void io_out32(uint16 port, unsigned int value) {
    __asm__ __volatile__(
        "outl %0, %%dx \n\t"
        "mfence   \n\t"
        :
        : "a"(value), "d"(port)
        : "memory");
}

/*
 * macros
 */

#define port_insw(port, buffer, nr)                                       \
    __asm__ __volatile__("cld;rep;insw;mfence;" ::"d"(port), "D"(buffer), \
                         "c"(nr)                                          \
                         : "memory")

#define port_outsw(port, buffer, nr)                                       \
    __asm__ __volatile__("cld;rep;outsw;mfence;" ::"d"(port), "S"(buffer), \
                         "c"(nr)                                           \
                         : "memory")

#define get_remainder(n, base)                \
    ({                                        \
        int __res;                            \
        __asm__("divq %%rcx"                  \
                : "=a"(n), "=d"(__res)        \
                : "0"(n), "1"(0), "c"(base)); \
        __res;                                \
    })

#endif /* _LIB_INCLUDED */
