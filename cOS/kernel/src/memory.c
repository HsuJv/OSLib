/*
 * memory.c
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

#include "memory.h"

#include "lib.h"
#include "printk.h"

#define flush_tlb_one(addr) \
    __asm__ __volatile__("invlpg (%0) \n\t" ::"r"(addr) : "memory")

#define flush_tlb()               \
    do {                          \
        uint64 tmpreg;            \
        __asm__ __volatile__(     \
            "movq %%cr3, %0 \n\t" \
            "movq %0, %%cr3 \n\t" \
            : "=r"(tmpreg)        \
            :                     \
            : "memory");          \
    } while (0)

static inline uint64 *get_gdt() {
    uint64 *tmp;
    __asm__ __volatile__("movq %%cr3, %0 \n\t" : "=r"(tmp) : : "memory");
    return tmp;
}

uint64 g_zone_dma = 0;
uint64 g_zone_normal = 0;   /* low 1GB RAM ,was mapped in pagetable */
uint64 g_zone_unmapped = 0; /* above 1GB RAM,unmapped in pagetable */
uint64 *g_cr3 = NULL;

static int64 page_init(Page *p, uint64 flags);
static int64 page_clean(Page *p);

static int64 page_init(Page *p, uint64 flags) {
    if (!p->attribute) {
        *(memory_management_struct.bits_map +
          (p->phy_addr >> PAGE_2M_SHIFT >> 6)) |=
            1UL << (p->phy_addr >> PAGE_2M_SHIFT) % 64;

        p->attribute = flags;
        p->ref_cnt += 1;
        p->zone_struct->page_using_cnt += 1;
        p->zone_struct->page_free_cnt -= 1;
        p->zone_struct->total_page_lnk += 1;
    } else if (p->attribute & PG_Referenced || p->attribute & PG_K_Share_To_U ||
               flags & PG_Referenced || flags & PG_K_Share_To_U) {
        p->attribute |= flags;
        p->ref_cnt += 1;
        p->zone_struct->total_page_lnk += 1;
    } else {
        *(memory_management_struct.bits_map +
          (p->phy_addr >> PAGE_2M_SHIFT >> 6)) |=
            1UL << (p->phy_addr >> PAGE_2M_SHIFT) % 64;
        p->attribute |= flags;
    }

    return 0;
}

static int64 page_clean(Page *p) {
    if (!p->attribute) {
        p->attribute = 0;
    } else if (p->attribute & PG_Referenced || p->attribute & PG_K_Share_To_U) {
        p->ref_cnt -= 1;
        p->zone_struct->total_page_lnk -= 1;

        if (!p->ref_cnt) {
            p->attribute = 0;
            p->zone_struct->page_using_cnt -= 1;
            p->zone_struct->page_free_cnt += 1;
        }
    } else {
        *(memory_management_struct.bits_map +
          (p->phy_addr >> PAGE_2M_SHIFT >> 6)) &=
            ~(1UL << (p->phy_addr >> PAGE_2M_SHIFT) % 64);

        p->attribute = 0;
        p->ref_cnt = 0;
        p->zone_struct->page_using_cnt -= 1;
        p->zone_struct->page_free_cnt += 1;
        p->zone_struct->total_page_lnk -= 1;
    }
}

/*
 * number <= 64
 * zone_select : zone select from dma, mapped in pagetale, unmapped in pagetable
 * page_flags: struct Page flags
 */
Page *alloc_pages(int32 zone_select, int32 number, uint64 page_flags) {
    uint64 i;
    uint64 page = 0;
    uint64 zone_start = 0;
    uint64 zone_end = 0;

    PAGE_DEBUG_PRINT("Entering");

    switch (zone_select) {
        case ZONE_DMA:
            zone_start = 0;
            zone_end = g_zone_dma;
            break;
        case ZONE_NORMAL:
            zone_start = g_zone_dma;
            zone_end = g_zone_normal;
            break;
        case ZONE_UNMAPPED:
            zone_start = g_zone_unmapped;
            zone_end = memory_management_struct.zones_size - 1;
            break;

        default:
            color_printk(RED, BLACK, "Alloc_pages error zone select index\n");
            break;
    }

    PAGE_DEBUG_PRINT("search zone start @ %lu end @ %lu", zone_start, zone_end);

    for (i = zone_start; i <= zone_end; i++) {
        Zone *z;
        uint64 j;
        uintptr start, end;
        uint64 tmp;

        if ((memory_management_struct.zones_struct + i)->page_free_cnt < number)
            continue;

        z = memory_management_struct.zones_struct + i;
        start = z->zone_start_addr >>
                PAGE_2M_SHIFT; /* the start page of this zone */
        end = z->zone_end_addr >> PAGE_2M_SHIFT; /* the end page of this zone */
        PAGE_DEBUG_PRINT("page search start at page %lu, end at page %lu",
                         start, end);
        /* make sure the search is stepping at the len of 64 */
        tmp = 64 - start % 64;

        for (j = start; j <= end; j += j % 64 ? tmp : 64) {
            uint64 *p = memory_management_struct.bits_map + (j >> 6);
            uint64 shift = j % 64; /* find the bit_map */
            uint64 k;

            for (k = shift; k < 64 - shift; k++) {
                /*
                 * *p >> k: the last pages of this 64-bit-map
                 * *(p+1) << (64-k): the first pags of the next 64-bit-map
                 * this test find the continious number pages that not in use
                 */
                if (!((*p >> k | *(p + 1) << (64 - k)) &
                      (number == 64 ? (uint64)-1 : (1UL << number) - 1))) {
                    uint64 l;
                    page = j + k - 1;
                    for (l = 0; l < number; l++) {
                        Page *x =
                            memory_management_struct.pages_struct + page + l;
                        page_init(x, page_flags);
                    }
                    PAGE_DEBUG_PRINT(
                        "find free page @ %lu %#016lx", page,
                        memory_management_struct.pages_struct + page);
                    goto find_free_pages;
                }
            }
        }
    }
    return NULL;
find_free_pages:
    return (Page *)(memory_management_struct.pages_struct + page);
}

void init_memory() {
    uint64 i, j;
    uint64 totel_mem = 0;
    E820 *p = NULL;

    PAGE_DEBUG_PRINT("Display Physics Address MAP\n");
    p = (E820 *)0xffff800000007e00;

    for (i = 0; i < 32; i++) {
#if PAGE_DEBUG
        printf("Address: %#018lx\tLength: %#018lx\tType: ", p->addr, p->len);

        switch (p->type) {
            case RAM:
                printf("RAM\n");
                totel_mem += p->len;
                break;
            case ROM:
                printf("ROM or Reserved\n");
                break;
            case ACPI_RECLAIM:
                printf("ACPI Reclaim Memory\n");
                break;
            case ACPI_NVS:
                printf("ACPI NVS Memroy\n");
                break;

            default:
                printf("Undefine\n");
                break;
        }
#endif
        memory_management_struct.e820[i].addr += p->addr;
        memory_management_struct.e820[i].len += p->len;
        memory_management_struct.e820[i].type = p->type;
        memory_management_struct.e820_len = i + 1;
        p++;
        if (p->type > ACPI_NVS || 0 == p->len || p->type < RAM) break;
    }

    PAGE_DEBUG_PRINT("OS can used total RAM: %#018lx\n", totel_mem);

#if PAGE_DEBUG
    totel_mem = 0;
    for (i = 0; i < memory_management_struct.e820_len; i++) {
        uint64 start, end;
        if (RAM != memory_management_struct.e820[i].type) continue;

        start = PAGE_2M_ALIGN(memory_management_struct.e820[i].addr);
        end = (memory_management_struct.e820[i].addr +
               memory_management_struct.e820[i].len) >>
              PAGE_2M_SHIFT << PAGE_2M_SHIFT;

        if (end <= start) continue;

        totel_mem += (end - start) >> PAGE_2M_SHIFT;
    }
    PAGE_DEBUG_PRINT("OS can used total 2M pages: %#010x=%010d\n", totel_mem,
                     totel_mem);
#endif

    totel_mem =
        memory_management_struct.e820[memory_management_struct.e820_len - 1]
            .addr +
        memory_management_struct.e820[memory_management_struct.e820_len - 1]
            .len;

    /* points to the aligned edge at the end of kernel */
    memory_management_struct.bits_map =
        (uint64 *)((memory_management_struct.end_brk + PAGE_4K_SIZE - 1) &
                   PAGE_4K_MASK);

    /* count that can be divided into 2M pages */
    memory_management_struct.bits_size = totel_mem >> PAGE_2M_SHIFT;
    memory_management_struct.bits_len =
        (((uint64)(totel_mem >> PAGE_2M_SHIFT) + sizeof(uint64) * 8 - 1) / 8) &
        (~(sizeof(uint64) - 1));
    /* set all memory in use */
    memset(memory_management_struct.bits_map, 0xff,
           memory_management_struct.bits_len);

#if PAGE_DEBUG
    PAGE_DEBUG_PRINT("after init bits_map: %p ",
                     memory_management_struct.bits_map);
    dump_bin(*memory_management_struct.bits_map);
    printf("\n");
#endif

    /* pages construction init */
    memory_management_struct.pages_struct =
        (Page *)(((uintptr)memory_management_struct.bits_map +
                  memory_management_struct.bits_len + PAGE_4K_SIZE - 1) &
                 PAGE_4K_MASK);

    memory_management_struct.pages_size = (totel_mem >> PAGE_2M_SHIFT);

    memory_management_struct.pages_len =
        ((totel_mem >> PAGE_2M_SHIFT) * sizeof(Page) + sizeof(uint64) - 1) &
        ~(sizeof(uint64) - 1);

    memset(memory_management_struct.pages_struct, 0x00,
           memory_management_struct.pages_len);

    /* zones construction init */
    memory_management_struct.zones_struct =
        (Zone *)(((uintptr)memory_management_struct.pages_struct +
                  memory_management_struct.pages_len + PAGE_4K_SIZE - 1) &
                 PAGE_4K_MASK);

    memory_management_struct.zones_size = 0;

    for (int i = 0; i < memory_management_struct.e820_len; i++) {
        uintptr start, end;
        Zone *z;
        Page *p;

        if (RAM != memory_management_struct.e820[i].type) continue;

        start = PAGE_2M_ALIGN(memory_management_struct.e820[i].addr);
        end = (memory_management_struct.e820[i].addr +
               memory_management_struct.e820[i].len) >>
              PAGE_2M_SHIFT << PAGE_2M_SHIFT;
        if (end <= start) continue;

        // zone init
        z = memory_management_struct.zones_struct +
            memory_management_struct.zones_size;
        memory_management_struct.zones_size++;

        memory_management_struct.zones_len +=
            (sizeof(Zone) + sizeof(uint64) - 1) & ~(sizeof(uint64) - 1);
        memset(z, 0x00, sizeof(Zone));

        z->zone_start_addr = start;
        z->zone_end_addr = end;
        z->zone_len = end - start;

        z->page_using_cnt = 0;
        z->page_free_cnt = z->zone_len >> PAGE_2M_SHIFT;
        z->total_page_lnk = 0;

        z->attribute = 0;
        z->GMD_struct = &memory_management_struct;
        z->page_len = z->zone_len >> PAGE_2M_SHIFT;
        z->page_group = (Page *)(memory_management_struct.pages_struct +
                                 (start >> PAGE_2M_SHIFT));

        /* page init */
        p = z->page_group;
        for (j = 0; j < z->page_len; j++, p++) {
            p->zone_struct = z;
            p->phy_addr = start + PAGE_2M_SIZE * j;
            p->attribute = 0;

            p->ref_cnt = 0;
            p->age = 0;
            /* mark the page as free */
            *(memory_management_struct.bits_map +
              (p->phy_addr >> PAGE_2M_SHIFT >> 6)) ^=
                1UL << (p->phy_addr >> PAGE_2M_SHIFT) % 64;
#if PAGE_DEBUG
            if (j < 10) {
                PAGE_DEBUG_PRINT("mark page %lu start at %lu as free", j,
                                 p->phy_addr);
                dump_bin(*(memory_management_struct.bits_map +
                           (p->phy_addr >> PAGE_2M_SHIFT >> 6)));
                printf("\n");
            }
#endif
        }
    }

    /* init address 0 to page struct 0 */
    memory_management_struct.pages_struct->zone_struct =
        memory_management_struct.zones_struct;
    memory_management_struct.pages_struct->phy_addr = 0UL;
    memory_management_struct.pages_struct->attribute = 0;
    memory_management_struct.pages_struct->ref_cnt = 0;
    memory_management_struct.pages_struct->age = 0;

#if PAGE_DEBUG
    /* print some info */
    color_printk(
        ORANGE, BLACK,
        "bits_map: %#018lx, bits_size: %#018lx, bits_length: %#018lx\n",
        memory_management_struct.bits_map, memory_management_struct.bits_size,
        memory_management_struct.bits_len);

    color_printk(
        ORANGE, BLACK,
        "pages_struct: %#018lx, pages_size: %#018lx, pages_length: %#018lx\n",
        memory_management_struct.pages_struct,
        memory_management_struct.pages_size,
        memory_management_struct.pages_len);

    color_printk(
        ORANGE, BLACK,
        "zones_struct: %#018lx, zones_size: %#018lx, zones_length: %#018lx\n",
        memory_management_struct.zones_struct,
        memory_management_struct.zones_size,
        memory_management_struct.zones_len);
#endif

    /* int g_zone_dma = 0; */    /* need rewrite in future */
    /* int g_zone_normal = 0; */ /* need rewrite in future */

    for (i = 0; i < memory_management_struct.zones_size;
         i++) /* need rewrite */ {
        Zone *z = memory_management_struct.zones_struct + i;
#if PAGE_DEBUG
        PAGE_DEBUG_PRINT(
            "zone start @: %#018lx, end @: %#018lx, len: %#018x, page_group: "
            "%018lx, page_len: %#018lx\n",
            z->zone_start_addr, z->zone_end_addr, z->zone_len, z->page_group,
            z->page_len);
#endif
        if (z->zone_start_addr == 0x100000000) g_zone_unmapped = i;
    }

    memory_management_struct.end_of_struct =
        (uint64)((uintptr)memory_management_struct.zones_struct +
                 memory_management_struct.zones_len + sizeof(uint64) * 32) &
        ~(sizeof(uint64) - 1);

    PAGE_DEBUG_PRINT(
        "start_code: %#018lx, end_code: %#018lx, end_data: %#018lx, end_brk: "
        "%#018lx, end_of_struct: %#018lx\n",
        memory_management_struct.start_code, memory_management_struct.end_code,
        memory_management_struct.end_data, memory_management_struct.end_brk,
        memory_management_struct.end_of_struct);

    i = Virt_To_Phy(memory_management_struct.end_of_struct) >> PAGE_2M_SHIFT;

    for (j = 0; j <= i; j++) {
        page_init(memory_management_struct.pages_struct + j,
                  PG_PTable_Maped | PG_Kernel_Init | PG_Active | PG_Kernel);
    }

    g_cr3 = get_gdt();

    PAGE_DEBUG_PRINT("Global CR3\t: %#018lx\n", g_cr3);
    PAGE_DEBUG_PRINT("*Global CR3\t: %#018lx\n",
                     *(uint64 *)Phy_To_Virt(g_cr3) & (uint64)~0xff);
    PAGE_DEBUG_PRINT(
        "**Global_CR3\t: %#018lx\n",
        *(uint64 *)Phy_To_Virt(*(uint64 *)Phy_To_Virt(g_cr3) & (uint64)~0xff) &
            (uint64)~0xff);

    for (i = 0; i < 10; i++) {
        *((uint64 *)Phy_To_Virt(g_cr3) + i) = 0UL;
    }
    flush_tlb();
    PAGE_DEBUG_PRINT("Done");
}

#undef flush_tlb_one
#undef flush_tlb