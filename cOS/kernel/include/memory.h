#ifndef _MEMORY_INCLUDED
#define _MEMORY_INCLUDED

#include "type.h"
#include "debug.h"

#if PAGE_DEBUG
#define PAGE_DEBUG_PRINT(fmt, ...)                                             \
    do {                                                                       \
        color_printk(RED, BLACK, "Page_debug(%s %d): " fmt "\n", __FUNCTION__, \
                     __LINE__, ##__VA_ARGS__);                                 \
    } while (0)
#else
#define PAGE_DEBUG_PRINT(...) \
    do {                      \
        ;                     \
    } while (0)
#endif

#ifndef Memory_Type
#define Memory_Type

#define RAM ((uint32)1)
#define ROM ((uint32)2)
#define RESERVED ((uint32)2)
#define ACPI_RECLAIM ((uint32)3)
#define ACPI_NVS ((uint32)4)

#endif

#ifndef PAGE_DEFINES
#define PAGE_DEFINES

#define PTRS_PER_PAGE (512)
#define PAGE_OFFSET ((uint64)0xffff800000000000)

#define PAGE_GDT_SHIFT (39)
#define PAGE_1G_SHIFT (30)
#define PAGE_2M_SHIFT (21)
#define PAGE_4K_SHIFT (12)

#define PAGE_1G_SIZE (1UL << PAGE_1G_SHIFT)
#define PAGE_2M_SIZE (1UL << PAGE_2M_SHIFT)
#define PAGE_4K_SIZE (1UL << PAGE_4K_SHIFT)

#define PAGE_1G_MASK (~(PAGE_1G_SIZE - 1))
#define PAGE_2M_MASK (~(PAGE_2M_SIZE - 1))
#define PAGE_4K_MASK (~(PAGE_4K_SIZE - 1))

#define PAGE_1G_ALIGN(addr) \
    (((uintptr)(addr) + PAGE_1G_SIZE - 1) & PAGE_1G_MASK)
#define PAGE_2M_ALIGN(addr) \
    (((uintptr)(addr) + PAGE_2M_SIZE - 1) & PAGE_2M_MASK)
#define PAGE_4K_ALIGN(addr) \
    (((uintptr)(addr) + PAGE_4K_SIZE - 1) & PAGE_4K_MASK)

#define Virt_To_Phy(addr) ((uintptr)(addr)-PAGE_OFFSET)
#define Phy_To_Virt(addr) ((void*)((uintptr)(addr) + PAGE_OFFSET))

#endif

typedef struct _E820 {
    uintptr addr;
    uint64 len;
    uint32 type;
} __attribute__((packed)) E820;

typedef struct _Global_Memory_Desc {
    E820 e820[32];   /* physical memory struct */
    uint64 e820_len; /* len of e820 */

    uint64* bits_map; /* page mapped bit-map */
    uint64 bits_size; /* pages count */
    uint64 bits_len;  /* len of bits_map */

    struct _Page* pages_struct;
    uint64 pages_size; /* count of page structs */
    uint64 pages_len;  /* length of page structs */

    struct _Zone* zones_struct;
    uint64 zones_size; /* count of zone structs */
    uint64 zones_len;  /* length of zone structs */

    /*
     * start_code: the start address of code section
     * end_code: the end address of code section
     * end_data: the end address of data section
     * end_brk: the end address of kernel
     */
    uintptr start_code, end_code, end_data, end_brk;

    uintptr end_of_struct;
} Global_Memory_Desc;

/* accessible memory sections */
#ifndef ZONE_ATTR
#define ZONE_ATTR

#define MAX_NR_ZONES 10 /* max zone */
#define ZONE_DMA 0x1
#define ZONE_NORMAL 0x2
#define ZONE_UNMAPPED 0x4

#endif

typedef struct _Zone {
    struct _Page* page_group; /* pages belong to this zone */
    uint64 page_len;          /* page count */
    uintptr zone_start_addr;  /* tha aligned address of the start page */
    uintptr zone_end_addr;    /* tha aligned address of the end page */
    uint64 zone_len;          /* length after align */
    uint64 attribute;

    Global_Memory_Desc* GMD_struct;

    uint64 page_using_cnt; /* pages in use */
    uint64 page_free_cnt;  /* pages free */
    uint64 total_page_lnk; /* reference conut (one physical page can be mapped
                              to lots of logical page, so total_page_lnk <>
                              page_using_cnt */
} Zone;

#ifndef PAGE_ATTR
#define PAGE_ATTR

/* page table attribute */
/* Fbit 63 Execution Disable: */
#define PAGE_XD (uint64)0x1000000000000000
/*  bit 12 Page Attribute Table */
#define PAGE_PAT (uint64)0x1000
/*  bit 8 Global Page:1,global;0,part */
#define PAGE_Global (uint64)0x0100
/*  bit 7 Page Size:1,big page;0,small page; */
#define PAGE_PS (uint64)0x0080
/*  bit 6 Dirty:1,dirty;0,clean; */
#define PAGE_Dirty (uint64)0x0040
/*  bit 5 Accessed:1,visited;0,unvisited; */
#define PAGE_Accessed (uint64)0x0020
/*  bit 4 Page Level Cache Disable */
#define PAGE_PCD (uint64)0x0010
/*  bit 3 Page Level Write Through */
#define PAGE_PWT (uint64)0x0008
/*  bit 2 User Supervisor:1,user and supervisor;0,supervisor; */
#define PAGE_U_S (uint64)0x0004
/*  bit 1 Read Write:1,read and write;0,read; */
#define PAGE_R_W (uint64)0x0002
/*  bit 0 Present:1,present;0,no present; */
#define PAGE_Present (uint64)0x0001
/*  1, 0 */
#define PAGE_KERNEL_GDT (PAGE_R_W | PAGE_Present)
/*  1, 0 */
#define PAGE_KERNEL_Dir (PAGE_R_W | PAGE_Present)
/*  7, 1, 0 */
#define PAGE_KERNEL_Page (PAGE_PS | PAGE_R_W | PAGE_Present)
/*  2, 1, 0 */
#define PAGE_USER_Dir (PAGE_U_S | PAGE_R_W | PAGE_Present)
/*  7, 2, 1, 0 */
#define PAGE_USER_Page (PAGE_PS | PAGE_U_S | PAGE_R_W | PAGE_Present)

/* struct page attribute (alloc_pages flags) */
#define PG_PTable_Maped 0x1
#define PG_Kernel_Init 0x2
#define PG_Referenced 0x4
#define PG_Dirty 0x8
#define PG_Active 0x10
#define PG_Up_To_Date 0x20
#define PG_Device 0x40
#define PG_Kernel 0x80
#define PG_K_Share_To_U 0x100
#define PG_Slab 0x200

#endif

/* accessible memory pages */
typedef struct _Page {
    struct _Zone* zone_struct; /* the zone this page belongs to */
    uintptr phy_addr;          /* page physical address */
    uint64 attribute;          /* page attribute */
    uint64 ref_cnt;            /* reference count */
    uint64 age;                /* create time */

} Page;

/* Page Map Level 4 */
typedef struct {
    uintptr pml4t;
} pml4t_t;
#define mk_mpl4t(addr, attr) ((uintptr)(addr) | (uintptr)(attr))
#define set_mpl4t(mpl4tptr, mpl4tval) (*(mpl4tptr) = (mpl4tval))

/* Page Directory Pointer Table */
typedef struct {
    uintptr pdpt;
} pdpt_t;
#define mk_pdpt(addr, attr) ((uintptr)(addr) | (uintptr)(attr))
#define set_pdpt(pdptptr, pdptval) (*(pdptptr) = (pdptval))

/* Page Directory Table */
typedef struct {
    uintptr pdt;
} pdt_t;
#define mk_pdt(addr, attr) ((uintptr)(addr) | (uintptr)(attr))
#define set_pdt(pdtptr, pdtval) (*(pdtptr) = (pdtval))

/* Page Table */
typedef struct {
    uintptr pt;
} pt_t;
#define mk_pt(addr, attr) ((uintptr)(addr) | (uintptr)(attr))
#define set_pt(ptptr, ptval) (*(ptptr) = (ptval))

/* data extern */
extern Global_Memory_Desc memory_management_struct;
extern uint64* g_cr3;

/* function extern */
void init_memory();
Page* alloc_pages(int32 zone_select, int32 number, uint64 page_flags);

#endif /* _MEMORY_INCLUDED */
