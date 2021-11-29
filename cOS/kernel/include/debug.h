#ifndef _DEBUG_INCLUDED
#define _DEBUG_INCLUDED

#if DEBUG
#define KERNEL_DEBUG 1
#define PAGE_DEBUG 0
#define INTERRUPT_DEBUG 0
#define TASK_DEBUG 1

#else
#define KERNEL_DEBUG 0
#define PAGE_DEBUG 0
#define INTERRUPT_DEBUG 0
#define TASK_DEBUG 0

#endif

#endif /* _DEBUG_INCLUDED */
