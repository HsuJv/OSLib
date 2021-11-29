/*
 * printk.c
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

#include <stdarg.h>

#include "font.h"
#include "lib.h"
#include "linkage.h"
#include "printk.h"

/*
 * global variables
 */
byte __buf[4096] = {0};

/*
 * static variables
 */

static position_s Pos;

/*
 * static functions
 */
static byte *__number(byte *str, int64 num, int32 base, int32 size,
                      int32 precision, int32 type);
static void __putchar(uint32 *fb, int32 Xsize, int32 x, int32 y, uint32 FRcolor,
                      uint32 BKcolor, const byte font);
static int32 __skip_atoi(const byte **s);
static int64 __vsprintf(byte *__buf, const byte *fmt, va_list args);
static void __print_char(uint32 FRcolor, uint32 BKcolor, const byte c);

static void __putchar(uint32 *fb, int32 Xsize, int32 x, int32 y, uint32 FRcolor,
                      uint32 BKcolor, const byte font) {
    int32 i = 0, j = 0;
    uint32 *addr = NULL;
    byte *fontp = NULL;
    int32 testval = 0;
    fontp = font_ascii[font];

    for (i = 0; i < 16; i++) {
        addr = fb + Xsize * (y + i) + x;
        testval = 0x100;
        for (j = 0; j < 8; j++) {
            testval = testval >> 1;
            if (*fontp & testval)
                *addr = FRcolor;
            else
                *addr = BKcolor;
            addr++;
        }
        fontp++;
    }
}

static int32 __skip_atoi(const byte **s) {
    int32 i = 0;

    while (is_digit(**s)) i = i * 10 + *((*s)++) - '0';
    return i;
}

static byte *__number(byte *str, int64 num, int32 base, int32 size,
                      int32 precision, int32 type) {
    byte c, sign, tmp[50];
    const byte *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int32 i;

    if (type & SMALL) digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (type & LEFT) type &= ~ZEROPAD;
    if (base < 2 || base > 36) return 0;
    c = (type & ZEROPAD) ? '0' : ' ';
    sign = 0;
    if (type & SIGN && num < 0) {
        sign = '-';
        num = -num;
    } else
        sign = (type & PLUS) ? '+' : ((type & SPACE) ? ' ' : 0);
    if (sign) size--;
    if (type & SPECIAL)
        if (base == 16)
            size -= 2;
        else if (base == 8)
            size--;
    i = 0;
    if (num == 0)
        tmp[i++] = '0';
    else
        while (num != 0) tmp[i++] = digits[get_remainder(num, base)];
    if (i > precision) precision = i;
    size -= precision;
    if (!(type & (ZEROPAD + LEFT)))
        while (size-- > 0) *str++ = ' ';
    if (sign) *str++ = sign;
    if (type & SPECIAL)
        if (base == 8)
            *str++ = '0';
        else if (base == 16) {
            *str++ = '0';
            *str++ = digits[33];
        }
    if (!(type & LEFT))
        while (size-- > 0) *str++ = c;

    while (i < precision--) *str++ = '0';
    while (i-- > 0) *str++ = tmp[i];
    while (size-- > 0) *str++ = ' ';
    return str;
}

static int64 __vsprintf(byte *__buf, const byte *fmt, va_list args) {
    byte *str, *s;
    int32 flags;
    int32 field_width;
    int32 precision;
    int32 len, i;

    int32 qualifier; /* 'h', 'l', 'L' or 'Z' for integer fields */

    for (str = __buf; *fmt; fmt++) {
        if (*fmt != '%') {
            *str++ = *fmt;
            continue;
        }
        flags = 0;
    repeat:
        fmt++;
        switch (*fmt) {
            case '-':
                flags |= LEFT;
                goto repeat;
            case '+':
                flags |= PLUS;
                goto repeat;
            case ' ':
                flags |= SPACE;
                goto repeat;
            case '#':
                flags |= SPECIAL;
                goto repeat;
            case '0':
                flags |= ZEROPAD;
                goto repeat;
        }

        /* get field width */

        field_width = -1;
        if (is_digit(*fmt))
            field_width = __skip_atoi(&fmt);
        else if (*fmt == '*') {
            fmt++;
            field_width = va_arg(args, int32);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        /* get the precision */

        precision = -1;
        if (*fmt == '.') {
            fmt++;
            if (is_digit(*fmt))
                precision = __skip_atoi(&fmt);
            else if (*fmt == '*') {
                fmt++;
                precision = va_arg(args, int32);
            }
            if (precision < 0) precision = 0;
        }

        qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L' || *fmt == 'Z') {
            qualifier = *fmt;
            fmt++;
        }

        switch (*fmt) {
            case 'c':

                if (!(flags & LEFT))
                    while (--field_width > 0) *str++ = ' ';
                *str++ = (byte)va_arg(args, int32);
                while (--field_width > 0) *str++ = ' ';
                break;

            case 's':

                s = va_arg(args, byte *);
                if (!s) s = '\0';
                len = strlen(s);
                if (precision < 0)
                    precision = len;
                else if (len > precision)
                    len = precision;

                if (!(flags & LEFT))
                    while (len < field_width--) *str++ = ' ';
                for (i = 0; i < len; i++) *str++ = *s++;
                while (len < field_width--) *str++ = ' ';
                break;

            case 'o':

                if (qualifier == 'l')
                    str = __number(str, va_arg(args, int64), 8, field_width,
                                   precision, flags);
                else
                    str = __number(str, va_arg(args, int32), 8, field_width,
                                   precision, flags);
                break;

            case 'p':

                if (field_width == -1) {
                    field_width = 2 * sizeof(void *);
                    flags |= ZEROPAD;
                }

                str = __number(str, (int64)va_arg(args, void *), 16,
                               field_width, precision, flags);
                break;

            case 'x':

                flags |= SMALL;

            case 'X':

                if (qualifier == 'l')
                    str = __number(str, va_arg(args, int64), 16, field_width,
                                   precision, flags);
                else
                    str = __number(str, va_arg(args, int32), 16, field_width,
                                   precision, flags);
                break;

            case 'd':
            case 'i':

                flags |= SIGN;
            case 'u':

                if (qualifier == 'l')
                    str = __number(str, va_arg(args, int64), 10, field_width,
                                   precision, flags);
                else
                    str = __number(str, va_arg(args, int32), 10, field_width,
                                   precision, flags);
                break;

            case '%':

                *str++ = '%';
                break;

            default:

                *str++ = '%';
                if (*fmt)
                    *str++ = *fmt;
                else
                    fmt--;
                break;
        }
    }
    *str = '\0';
    return (str - __buf);
}

static void __print_char(const uint32 FRcolor, const uint32 BKcolor,
                         const byte c) {
    int32 tab_cnt = 0;

    switch (c) {
        case '\n':
            Pos.YPosition++;
            Pos.XPosition = 0;

            break;
        case '\b':
            Pos.XPosition--;
            if (Pos.XPosition < 0) {
                Pos.XPosition =
                    (Pos.XResolution / Pos.XCharSize - 1) * Pos.XCharSize;
                Pos.YPosition--;
                if (Pos.YPosition < 0)
                    Pos.YPosition =
                        (Pos.YResolution / Pos.YCharSize - 1) * Pos.YCharSize;
            }
            __putchar(Pos.FB_addr, Pos.XResolution,
                      Pos.XPosition * Pos.XCharSize,
                      Pos.YPosition * Pos.YCharSize, FRcolor, BKcolor, ' ');
            break;
        case '\t':
            tab_cnt = ((Pos.XPosition + 8) & ~(8 - 1)) - Pos.XPosition;
            while (tab_cnt--) {
                __putchar(Pos.FB_addr, Pos.XResolution,
                          Pos.XPosition * Pos.XCharSize,
                          Pos.YPosition * Pos.YCharSize, FRcolor, BKcolor, ' ');
                Pos.XPosition++;
            }
            break;

        default:
            __putchar(Pos.FB_addr, Pos.XResolution,
                      Pos.XPosition * Pos.XCharSize,
                      Pos.YPosition * Pos.YCharSize, FRcolor, BKcolor, c);
            Pos.XPosition++;
            break;
    }

    if (Pos.XPosition >= (Pos.XResolution / Pos.XCharSize)) {
        Pos.YPosition++;
        Pos.XPosition = 0;
    }
    if (Pos.YPosition >= (Pos.YResolution / Pos.YCharSize)) {
        Pos.YPosition = 0;
    }
}

int64 color_printk(const uint32 FRcolor, const uint32 BKcolor, const byte *fmt,
                   ...) {
    int64 i = 0;
    int64 count = 0;
    va_list args;
    va_start(args, fmt);

    i = __vsprintf(__buf, fmt, args);

    va_end(args);

    for (count = 0; count < i; count++)
        __print_char(FRcolor, BKcolor, *(__buf + count));

    return i;
}

void set_pos(const position_s *new) { memcpy(&Pos, new, sizeof(position_s)); }

void putchar(const byte ch) { __print_char(WHITE, BLACK, ch); }

void dump_bin(const uint64 i) {
    putchar('0');
    putchar('b');

    int8 j = sizeof(i) * 8 - 1;
    while (j >= 0) {
        if (i & (1UL << j))
            putchar('1');
        else
            putchar('0');
        j--;
    }
}

void dump_hex(const uint64 i) { printf("%#016x", i); }