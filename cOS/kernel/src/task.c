/*
 * task.c
 * Copyright (C) 2019 by jovi Hsu(jv.hsu@outlook.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "task.h"

#include "gate.h"
#include "printk.h"

#define INIT_TASK(tsk)                                                      \
    {                                                                       \
        .state = TASK_UNINTERRUPTIBLE, .flags = PF_KTHREAD, .mm = &init_mm, \
        .thread = &init_thread, .addr_limit = 0xffff800000000000, .pid = 0, \
        .counter = 1, .signal = 0, .priority = 0                            \
    }

#define INIT_TSS                                                               \
    {                                                                          \
        .reserved0 = 0,                                                        \
        .rsp0 = (uint64)(init_task_union.stack + STACK_SIZE / sizeof(uint64)), \
        .rsp1 = (uint64)(init_task_union.stack + STACK_SIZE / sizeof(uint64)), \
        .rsp2 = (uint64)(init_task_union.stack + STACK_SIZE / sizeof(uint64)), \
        .reserved1 = 0, .ist1 = 0xffff800000007c00,                            \
        .ist2 = 0xffff800000007c00, .ist3 = 0xffff800000007c00,                \
        .ist4 = 0xffff800000007c00, .ist5 = 0xffff800000007c00,                \
        .ist6 = 0xffff800000007c00, .ist7 = 0xffff800000007c00,                \
        .reserved2 = 0, .reserved3 = 0, .iomapbaseaddr = 0                     \
    }

#if 1  // externs for file varibles
extern task_union init_task_union;
static mm_struct init_mm;
static thread_struct init_thread;
#endif

task_union init_task_union __attribute__((__section__(".data.init_task"))) = {
    INIT_TASK(init_task_union.task)};
static task_struct *init_task[NR_CPUS] = {&init_task_union.task, 0};
static mm_struct init_mm = {0};
static thread_struct init_thread = {
    .rsp0 = (uint64)(init_task_union.stack + STACK_SIZE / sizeof(uint64)),
    .rsp = (uint64)(init_task_union.stack + STACK_SIZE / sizeof(uint64)),
    .fs = KERNEL_DS,
    .gs = KERNEL_DS,
    .cr2 = 0,
    .trap_nr = 0,
    .error_code = 0};
static tss_struct init_tss[NR_CPUS] = {[0 ... NR_CPUS - 1] = INIT_TSS};

/*
 * the struct is forced to be located at a 32KB aligned addr
 * (as declared in kernel.lds)
 * as we excepted, all process will have their pcb located at
 * rsp & ~32k
 */
static inline task_struct *get_current() {
    task_struct *current = NULL;
    __asm__ __volatile__("andq %%rsp, %0    \n\t"
                         : "=r"(current)
                         : "0"(~32767UL));
    return current;
}

#define current get_current()

#define GET_CURRENT                  \
    "movq    %rsp,    %rbx     \n\t" \
    "andq    $-32768, %rbx     \n\t"
/*
 * push %rbp, %rax
 * mov %rsp, prev->thread->rsp
 * mov next->thread->rsp, %rsp
 * rax = the return address while taking slot again
 * mov %rax, prev->thread->rip
 * push next->thread->rip ; the rsp has been restored
 * jmp __switch_to
 *
 * so we save the current rip and rsp in the pcb,
 * and alter the rsp and rip to the next pcb's
 * then we jump to the inline function __switch_to
 * for further process
 * prev in rdi and next in rsi,
 * which aligns the parameter taber of __switch_to
 */
#define switch_to(prev, next)                                            \
    do {                                                                 \
        __asm__ __volatile__(                                            \
            "pushq    %%rbp             \n\t"                            \
            "pushq    %%rax             \n\t"                            \
            "movq     %%rsp,    %0      \n\t"                            \
            "movq     %2,       %%rsp   \n\t"                            \
            "leaq     1f(%%rip),%%rax   \n\t"                            \
            "movq     %%rax,    %1      \n\t"                            \
            "pushq    %3                \n\t"                            \
            "jmp      __switch_to       \n\t"                            \
            "1:                         \n\t"                            \
            "popq     %%rax             \n\t"                            \
            "popq     %%rbp             \n\t"                            \
            : "=m"(prev->thread->rsp), "=m"(prev->thread->rip)           \
            : "m"(next->thread->rsp), "m"(next->thread->rip), "D"(prev), \
              "S"(next)                                                  \
            : "memory");                                                 \
    } while (0)

/* cannot be static since we will jmp here in switch_to routine */
/*
 * set the tss (task state segment)
 * save fs, gs
 * set new fs, gs
 * after the routine, a ret instruct will start the next process
 * (since next -> rip is pushed)
 */
void __switch_to(task_struct *prev, task_struct *next) {
    TASK_DEBUG_PRINT("=====enter=====");
    init_tss[0].rsp0 = next->thread->rsp0;

    set_tss64(init_tss[0].rsp0, init_tss[0].rsp1, init_tss[0].rsp2,
              init_tss[0].ist1, init_tss[0].ist2, init_tss[0].ist3,
              init_tss[0].ist4, init_tss[0].ist5, init_tss[0].ist6,
              init_tss[0].ist7);

    __asm__ __volatile__("movq    %%fs,    %0 \n\t" : "=a"(prev->thread->fs));
    __asm__ __volatile__("movq    %%gs,    %0 \n\t" : "=a"(prev->thread->gs));

    __asm__ __volatile__("movq    %0,    %%fs \n\t" ::"a"(next->thread->fs));
    __asm__ __volatile__("movq    %0,    %%gs \n\t" ::"a"(next->thread->gs));

    TASK_DEBUG_PRINT("prev->thread->rsp0:%#018lx", prev->thread->rsp0);
    TASK_DEBUG_PRINT("next->thread->rsp0:%#018lx", next->thread->rsp0);
    TASK_DEBUG_PRINT("=====exit=====");
#if TASK_DEBUG
    printf("curren:\n");
    printf("rip: %#018lx", prev->thread->rip);
    printf(" rsp: %#018lx", prev->thread->rsp);
    printf("\nfs: %#018lx", prev->thread->fs);
    printf(" gs: %#018lx\n", prev->thread->gs);
    printf("next:\n");
    printf("rip: %#018lx", next->thread->rip);
    printf(" rsp: %#018lx", next->thread->rsp);
    printf("\nfs: %#018lx", next->thread->fs);
    printf(" gs: %#018lx\n", next->thread->gs);
#endif
}

uint64 init(uint64 arg) {
    TASK_DEBUG_PRINT("=====enter=====");
    TASK_DEBUG_PRINT("init task is running,arg:%#018lx", arg);
    TASK_DEBUG_PRINT("=====exit=====");
    return 1;
}

uint64 do_fork(struct pt_regs *regs, uint64 clone_flags, uint64 stack_start,
               uint64 stack_size) {
    task_struct *tsk = NULL;
    thread_struct *thd = NULL;
    Page *p = NULL;

    TASK_DEBUG_PRINT("=====enter=====");

    TASK_DEBUG_PRINT("alloc_pages, bitmap:%#018lx",
                     *memory_management_struct.bits_map);

    p = alloc_pages(ZONE_NORMAL, 1, PG_PTable_Maped | PG_Active | PG_Kernel);

    TASK_DEBUG_PRINT("alloc_pages, bitmap:%#018lx",
                     *memory_management_struct.bits_map);

    tsk = (task_struct *)Phy_To_Virt(p->phy_addr);

    TASK_DEBUG_PRINT("struct task_struct address:%#018lx", (uintptr)tsk);

    memset(tsk, 0, sizeof(*tsk));
    *tsk = *(current);

    list_init(&tsk->list);
    list_add_to_before(&init_task_union.task.list, &tsk->list);
    tsk->pid++;
    tsk->state = TASK_UNINTERRUPTIBLE;

    thd = (thread_struct *)(tsk + 1);
    tsk->thread = thd;

    memcpy((void *)((uint64)tsk + STACK_SIZE - sizeof(struct pt_regs)), regs,
           sizeof(struct pt_regs));

    thd->rsp0 = (uint64)tsk + STACK_SIZE;
    thd->rip = regs->rip;
    thd->rsp = (uint64)tsk + STACK_SIZE - sizeof(struct pt_regs);
    TASK_DEBUG_PRINT("fork rip:%#018lx rsp:%#018lx", thd->rip, thd->rsp);

    if (!(tsk->flags & PF_KTHREAD))
        thd->rip = regs->rip = (uint64)ret_from_intr;

    tsk->state = TASK_RUNNING;
    TASK_DEBUG_PRINT("=====exit=====");
    return 0;
}

uint64 do_exit(uint64 code) {
    TASK_DEBUG_PRINT("=====enter=====");
    TASK_DEBUG_PRINT("exit task is running,arg:%#018lx", code);
    while (1)
        ;
    TASK_DEBUG_PRINT("=====exit=====");
}

extern void kernel_thread_func(void);
__asm__(
    ".global kernel_thread_func \n\t"
    "kernel_thread_func:        \n\t"
    "    popq    %r15           \n\t"
    "    popq    %r14           \n\t"
    "    popq    %r13           \n\t"
    "    popq    %r12           \n\t"
    "    popq    %r11           \n\t"
    "    popq    %r10           \n\t"
    "    popq    %r9            \n\t"
    "    popq    %r8            \n\t"
    "    popq    %rbx           \n\t"
    "    popq    %rcx           \n\t"
    "    popq    %rdx           \n\t"
    "    popq    %rsi           \n\t"
    "    popq    %rdi           \n\t"
    "    popq    %rbp           \n\t"
    "    popq    %rax           \n\t"
    "    movq    %rax,    %ds   \n\t"
    "    popq    %rax           \n\t"
    "    movq    %rax,    %es   \n\t"
    "    popq    %rax           \n\t"
    "    addq    $0x38,   %rsp  \n\t"
    "    movq    %rdx,    %rdi  \n\t"
    "    callq   *%rbx          \n\t"
    "    movq    %rax,    %rdi  \n\t"
    "    callq   do_exit        \n\t");

uint64 kernel_thread(uintptr (*fn)(uintptr), uintptr arg, uintptr flags) {
    TASK_DEBUG_PRINT("=====enter=====");
    struct pt_regs regs;
    memset(&regs, 0, sizeof(regs));

    regs.rbx = (uintptr)fn;
    regs.rdx = (uintptr)arg;

    regs.ds = KERNEL_DS;
    regs.es = KERNEL_DS;
    regs.cs = KERNEL_CS;
    regs.ss = KERNEL_DS;
    regs.rflags = (1 << 9);
    regs.rip = (uintptr)kernel_thread_func;
    TASK_DEBUG_PRINT("=====exit=====");
    TASK_DEBUG_PRINT("kernel_thread_func @ %p", kernel_thread_func);
    return do_fork(&regs, flags, 0, 0);
}

void task_init() {
    struct task_struct *p = NULL;
    TASK_DEBUG_PRINT("=====enter=====");
    init_mm.pgd = (pml4t_t *)g_cr3;

    init_mm.start_code = memory_management_struct.start_code;
    init_mm.end_code = memory_management_struct.end_code;

    init_mm.start_data = (uintptr)&_data;
    init_mm.end_data = memory_management_struct.end_data;

    init_mm.start_rodata = (uintptr)&_rodata;
    init_mm.end_rodata = (uintptr)&_erodata;

    init_mm.start_brk = 0;
    init_mm.end_brk = memory_management_struct.end_brk;

    init_mm.start_stack = _stack_start;

    //    init_thread,init_tss
    set_tss64(init_thread.rsp0, init_tss[0].rsp1, init_tss[0].rsp2,
              init_tss[0].ist1, init_tss[0].ist2, init_tss[0].ist3,
              init_tss[0].ist4, init_tss[0].ist5, init_tss[0].ist6,
              init_tss[0].ist7);

    init_tss[0].rsp0 = init_thread.rsp0;

    list_init(&init_task_union.task.list);

    kernel_thread(init, 10, CLONE_FS | CLONE_FILES | CLONE_SIGNAL);

    init_task_union.task.state = TASK_RUNNING;

    p = container_of(list_next(&current->list), task_struct, list);
    TASK_DEBUG_PRINT("=====exit=====");
    switch_to(current, p);
}