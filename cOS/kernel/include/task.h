#ifndef _TASK_INCLUDED
#define _TASK_INCLUDED

#include "cpu.h"
#include "debug.h"
#include "lib.h"
#include "linkage.h"
#include "memory.h"
#include "ptrace.h"

#if TASK_DEBUG
#define TASK_DEBUG_PRINT(fmt, ...)                                             \
    do {                                                                       \
        color_printk(RED, BLACK, "Task_debug(%s %d): " fmt "\n", __FUNCTION__, \
                     __LINE__, ##__VA_ARGS__);                                 \
    } while (0)
#else
#define TASK_DEBUG_PRINT(...) \
    do {                      \
        ;                     \
    } while (0)
#endif

#define KERNEL_CS (0x08)
#define KERNEL_DS (0x10)

#define USER_CS (0x28)
#define USER_DS (0x30)

#define CLONE_FS (1 << 0)
#define CLONE_FILES (1 << 1)
#define CLONE_SIGNAL (1 << 2)

#define STACK_SIZE (32768)

extern byte _text;
extern byte _etext;
extern byte _data;
extern byte _edata;
extern byte _rodata;
extern byte _erodata;
extern byte _bss;
extern byte _ebss;
extern byte _end;

extern uint64 _stack_start;
extern void ret_from_intr();

#define TASK_RUNNING (1 << 0)
#define TASK_INTERRUPTIBLE (1 << 1)
#define TASK_UNINTERRUPTIBLE (1 << 2)
#define TASK_ZOMBIE (1 << 3)
#define TASK_STOPPED (1 << 4)

typedef struct mm_struct {
    pml4t_t *pgd;  // page table point
    uintptr start_code, end_code;
    uintptr start_data, end_data;
    uintptr start_rodata, end_rodata;
    uintptr start_brk, end_brk;
    uintptr start_stack;
} mm_struct;

#define PF_KTHREAD (1 << 0)

typedef struct thread_struct {
    uint64 rsp0;  // in tss
    uint64 rip;
    uint64 rsp;
    uint64 fs;
    uint64 gs;
    uint64 cr2;
    uint64 trap_nr;
    uint64 error_code;
} thread_struct;

/* PCB */
typedef struct task_struct {
    struct List list;      /* de-queue */
    volatile uint64 state; /* running, ready, blocked, etc */
    uint64 flags;          /* process, thread, kernel, etc */
    /* mm and thread
     * take the responsibility to save the sense while process de-scheduler */
    struct mm_struct *mm;         /* memory page table and segment */
    struct thread_struct *thread; /* sense while de-schelduler */
    /*0x0000,0000,0000,0000 - 0x0000,7fff,ffff,ffff user*/
    /*0xffff,8000,0000,0000 - 0xffff,ffff,ffff,ffff kernel*/
    uint64 addr_limit;
    uint64 pid;      /* process id */
    uint64 counter;  /* time slot that process can use */
    uint64 signal;   /* signal belongs to process */
    uint64 priority; /* priority */
} task_struct;

/*
 * Similiar to Linux kernel
 * the kernel stack shares the same address with pcb
 */
typedef union task_union {
    struct task_struct task;
    uint64 stack[STACK_SIZE / sizeof(uint64)];
} __attribute__((aligned(8))) task_union;  // 8Bytes align

typedef struct tss_struct {
    uint32 reserved0;
    uint64 rsp0;
    uint64 rsp1;
    uint64 rsp2;
    uint64 reserved1;
    uint64 ist1;
    uint64 ist2;
    uint64 ist3;
    uint64 ist4;
    uint64 ist5;
    uint64 ist6;
    uint64 ist7;
    uint64 reserved2;
    uint16 reserved3;
    uint16 iomapbaseaddr;
} __attribute__((packed)) tss_struct;

uint64 do_fork(struct pt_regs *regs, uint64 clone_flags, uint64 stack_start,
               uint64 stack_size);
void task_init();

#endif /* _TASK_INCLUDED */
