/*  Taken from
 *  linux/kernel/vsprintf.c
 *
 *  (C) 1991  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */

#include <console.h>
#include <lib/string.h>
#include <stdarg.h>
#include <stdint.h>

namespace console
{
static int num_printed;
static char buffer[1024];
static char* cur_loc = buffer;

static inline bool isdigit(char c)
{
    return c >= '0' && c <= '9';
}

// put with count
static inline void put_n(char c)
{
    if (cur_loc > buffer+1022) {
        puts(buffer);
        cur_loc = buffer;
    } else *cur_loc++ = c;
    num_printed++;
}

static inline int skip_atoi(const char **s)
{
    int i=0;

    while (isdigit(**s))
        i = i*10 + *((*s)++) - '0';
    return i;
}

#define ZEROPAD 1         /* pad with zero */
#define SIGN    2         /* unsigned/signed long */
#define PLUS    4         /* show plus */
#define SPACE   8         /* space if plus */
#define LEFT    16        /* left justified */
#define SPECIAL 32        /* 0x */
#define SMALL   64        /* use 'abcdef' instead of 'ABCDEF' */

static void number(int num, int base, int size, int precision, int type)
{
    if (base<2 || base>36)
        return;

    char c,sign, tmp[65];
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    int i;

    if (type&SMALL) digits = "0123456789abcdefghijklmnopqrstuvwxyz";
    if (type&LEFT) type &= ~ZEROPAD;
    c = (type & ZEROPAD) ? '0' : ' ' ;
    if (type&SIGN && num<0) {
        sign = '-';
        num = -num;
    } else
        sign=(type&PLUS) ? '+' : ((type&SPACE) ? ' ' : 0);
    if (sign) size--;
    if (type&SPECIAL) {
        if (base==16) size -= 2;
        else if (base==8) size--;
        }
    i=0;
    if (num==0)
        tmp[i++]='0';
    else {
        while (num) {
            tmp[i++] = digits[(unsigned int)num % (unsigned int)base];
            num = (int) ((unsigned int)num/(unsigned int)base);
        }
    }
    if (i>precision) precision=i;
    size -= precision;
    if (!(type&(ZEROPAD+LEFT)))
        while(size-->0)
            put_n(' ');
    if (sign)
        put_n(sign);
    if (type&SPECIAL) {
        if (base==8)
            put_n('0');
        else if (base==16) {
            put_n('0');
            put_n(digits[33]);
        }
        }
    if (!(type&LEFT))
        while(size-->0)
            put_n(c);
    while(i<precision--)
        put_n('0');
    while(i-->0)
        put_n(tmp[i]);
    while(size-->0)
        put_n(' ');
}

static int _vprintf(const char *fmt, va_list args)
{
    int len;
    int i;
    char *s;
    int* ip;

    int flags;        /* flags to number() */

    int field_width;    /* width of output_n field */
    int precision;        /* min. # of digits for integers; max
                   number of chars for from string */
    //int qualifier;    /* 'h', 'l', or 'L' for integer fields */

    num_printed = 0;
    buffer[1023] = '\0';
    cur_loc = buffer;

    for (;*fmt ; ++fmt) {
        if (*fmt != '%') {
            put_n(*fmt);
            continue;
        }
            
        /* process flags */
        flags = 0;
        repeat:
            ++fmt;        /* this also skips first '%' */
            switch (*fmt) {
                case '-': flags |= LEFT; goto repeat;
                case '+': flags |= PLUS; goto repeat;
                case ' ': flags |= SPACE; goto repeat;
                case '#': flags |= SPECIAL; goto repeat;
                case '0': flags |= ZEROPAD; goto repeat;
                }
        
        /* get field width */
        field_width = -1;
        if (isdigit(*fmt))
            field_width = skip_atoi(&fmt);
        else if (*fmt == '*') {
            /* it's the next argument */
            field_width = va_arg(args, int);
            if (field_width < 0) {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

        /* get the precision */
        precision = -1;
        if (*fmt == '.') {
            ++fmt;    
            if (isdigit(*fmt))
                precision = skip_atoi(&fmt);
            else if (*fmt == '*') {
                /* it's the next argument */
                precision = va_arg(args, int);
            }
            if (precision < 0)
                precision = 0;
        }

        /* get the conversion qualifier */
        //qualifier = -1;
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L') {
            //qualifier = *fmt;
            ++fmt;
        }

        switch (*fmt) {
        case 'c':
            if (!(flags & LEFT))
                while (--field_width > 0)
                    put_n(' ');
            put_n((char) va_arg(args, int));
            while (--field_width > 0)
                put_n(' ');
            break;

        case 's':
            s = va_arg(args, char *);
            len = strlen(s);
            if (precision < 0)
                precision = len;
            else if (len > precision)
                len = precision;

            if (!(flags & LEFT))
                while (len < field_width--)
                    put_n(' ');
            for (i = 0; i < len; ++i)
                put_n(*s++);
            while (len < field_width--)
                put_n(' ');
            break;

        case 'o':
            number(va_arg(args, unsigned long), 8,
                field_width, precision, flags);
            break;

        case 'p':
            if (field_width == -1) {
                field_width = 8;
                flags |= ZEROPAD;
            }
            number((unsigned long) va_arg(args, void *), 16, field_width, precision, flags);
            break;

        case 'x':
            flags |= SMALL;
        case 'X':
            number(va_arg(args, unsigned long), 16, field_width, precision, flags);
            break;

        case 'd':
        case 'i':
            flags |= SIGN;
        case 'u':
            number(va_arg(args, unsigned long), 10,
                field_width, precision, flags);
            break;

        case 'n':
            ip = va_arg(args, int *);
            *ip = num_printed;
            break;

        default:
            if (*fmt != '%')
                put_n('%');
            if (*fmt)
                put_n(*fmt);
            else
                --fmt;
            break;
        }
    }

    if (cur_loc != buffer) {
        *cur_loc = '\0';
        puts(buffer);
    }

    return num_printed;
}

int printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int res = _vprintf(fmt, args);
    va_end(args);
    return res;
}

}
