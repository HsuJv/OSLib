#include "debug.h"
#include "gate.h"
#include "interrupt.h"
#include "memory.h"
#include "printk.h"
#include "task.h"
#include "trap.h"

#if KERNEL_DEBUG
#define KERNEL_DEBUG_PRINT(fmt, ...)                                  \
    do {                                                              \
        color_printk(PURPLE, BLACK, "KERNEL_DEBUG(%s %d): " fmt "\n", \
                     __FUNCTION__, __LINE__, ##__VA_ARGS__);          \
    } while (0)
#else
#define KERNEL_DEBUG_PRINT(...) \
    do {                        \
        ;                       \
    } while (0)
#endif

/* =========== Global structures start =========== */
/* memory sturct */
Global_Memory_Desc memory_management_struct = {{0}, 0};
/* =========== Global structures end =========== */

#if 0 /* already have in task.h */
/* externs */
extern byte _text;
extern byte _etext;
extern byte _edata;
extern byte _end;
#endif

static void resolution_init() {
    position_s pos_tmp;

    pos_tmp.XResolution = 1440;
    pos_tmp.YResolution = 900;

    pos_tmp.XPosition = 0;
    pos_tmp.YPosition = 0;

    pos_tmp.XCharSize = 8;
    pos_tmp.YCharSize = 16;

    pos_tmp.FB_addr = (int *)0xffff800000a00000;
    pos_tmp.FB_length =
        ((uint64)(pos_tmp.XResolution * pos_tmp.YResolution * 4) +
         PAGE_4K_SIZE - 1) &
        PAGE_4K_MASK;

    set_pos(&pos_tmp);
}

void kernel_start(void) {
    resolution_init();

    int i;

    load_TR(8);

    // set_tss64(0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00,
    //           0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00,
    //           0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00,
    //           0xffff800000007c00);
    set_tss64(_stack_start, _stack_start, _stack_start, 0xffff800000007c00,
              0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00,
              0xffff800000007c00, 0xffff800000007c00, 0xffff800000007c00);

    init_sys_vector();
    KERNEL_DEBUG_PRINT("sys vector inited\n");

    memory_management_struct.start_code = (uintptr)&_text;
    memory_management_struct.end_code = (uintptr)&_etext;
    memory_management_struct.end_data = (uintptr)&_edata;
    memory_management_struct.end_brk = (uintptr)&_end;

    init_memory();
    KERNEL_DEBUG_PRINT("memory inited\n");

#if 0 && PAGE_DEBUG
    PAGE_DEBUG_PRINT("bits_map: ");
    dump_bin(*memory_management_struct.bits_map);
    printf("\n");
    PAGE_DEBUG_PRINT("bits_map + 1: ");
    dump_bin(*(memory_management_struct.bits_map + 1));
    printf("\n");
    Page *p =
        alloc_pages(ZONE_NORMAL, 64, PG_PTable_Maped | PG_Active | PG_Kernel);
    PAGE_DEBUG_PRINT("after bits_map: ");
    dump_bin(*memory_management_struct.bits_map);
    printf("\n");
    PAGE_DEBUG_PRINT("after bits_map + 1: ");
    dump_bin(*(memory_management_struct.bits_map + 1));
    printf("\n");
    if (p)
        for (i = 0; i < 64; i++) {
            printf("page %d\t", i);
            printf("attribute %#018lx\t", (p + i)->attribute);
            printf("addr %#018lx\t", (p + i)->phy_addr);
            if (i && i % 2) printf("\n");
        }
    else {
        PAGE_DEBUG_PRINT("something error");
    }
#endif
    init_interrupt();
    KERNEL_DEBUG_PRINT("interrupt inited\n");

    task_init();
    KERNEL_DEBUG_PRINT("task inited\n");

    while (1)
        ;
}