#ifndef _PRINTK_INCLUDED
#define _PRINTK_INCLUDED

#include "type.h"

#define ZEROPAD 1  /* pad with zero */
#define SIGN 2     /* unsigned/signed long */
#define PLUS 4     /* show plus */
#define SPACE 8    /* space if plus */
#define LEFT 16    /* left justified */
#define SPECIAL 32 /* 0x */
#define SMALL 64   /* use 'abcdef' instead of 'ABCDEF' */

#define is_digit(c) ((c) >= '0' && (c) <= '9')

#define WHITE (0x00ffffff)
#define BLACK (0x00000000)
#define RED (0x00ff0000)
#define ORANGE (0x00ff8000)
#define YELLOW (0x00ffff00)
#define GREEN (0x0000ff00)
#define BLUE (0x000000ff)
#define INDIGO (0x0000ffff)
#define PURPLE (0x008000ff)

#define printf(fmt, ...)                                \
    do {                                                \
        color_printk(WHITE, BLACK, fmt, ##__VA_ARGS__); \
    } while (0)

/*
 * export variables
 */
extern byte buf[4096];

/* structs */
typedef struct position {
    int32 XResolution;
    int32 YResolution;

    int32 XPosition;
    int32 YPosition;

    int32 XCharSize;
    int32 YCharSize;

    uint32 *FB_addr;
    uint64 FB_length;
} position_s;

/*
 * export functions
 */

int64 color_printk(const uint32 FRcolor, const uint32 BKcolor, const byte *fmt,
                   ...);
void putchar(const byte ch);
void set_pos(const position_s *);
void dump_hex(const uint64 i);
void dump_bin(const uint64 i);

#endif /* _PRINTK_INCLUDED */
