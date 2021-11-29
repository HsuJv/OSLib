#ifndef _INTERRUPT_INCLUDED
#define _INTERRUPT_INCLUDED

#include "debug.h"
#include "type.h"

#if INTERRUPT_DEBUG
#define INTERRUPT_DEBUG_PRINT(fmt, ...)                               \
    do {                                                              \
        color_printk(RED, BLACK, "Interrupt_debug(%s %d): " fmt "\n", \
                     __FUNCTION__, __LINE__, ##__VA_ARGS__);          \
    } while (0)
#else
#define INTERRUPT_DEBUG_PRINT(...) \
    do {                           \
        ;                          \
    } while (0)
#endif
void init_interrupt();
void do_IRQ(uint64 regs, uint64 nr);

#endif /* _INTERRUPT_INCLUDED */
