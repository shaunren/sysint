/* PS/2 keyboard.
   Copyright (C) 2015 Shaun Ren.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <console.h>
#include <isr.h>
#include <console.h>
#include <ports.h>
#include <lib/klib.h>
#include <lib/condvar.h>
#include "keycodes.h"

namespace devices
{
namespace keyboard
{

static uint8_t buffer[4096];
static size_t buffer_begin = 0;
static size_t buffer_end = 0;

static uint8_t kbdus[128] =
{
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '`',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

constexpr int MODI_CTRL  = 0;
constexpr int MODI_ALT   = 1;
constexpr int MODI_LSHIFT = 2;
constexpr int MODI_RSHIFT = 3;
static bool modifier_pressed[4];  // Ctrl, Alt, LShift, RShift

static char shift_table[128];

static process::condvar buffer_nonempty;

static void read_scancode()
{
    if ((buffer_end + 1) % sizeof_array(buffer) == buffer_begin) {
        return; // buffer full, ignore
    }

    uint8_t sc = inb(0x60);

    char ch = 0;
    if (sc & 0x80) {
        // key released
        sc = sc & 0x7f;
        if (sc == KEY_CNTRL)
            modifier_pressed[MODI_CTRL] = false;
        else if (sc == KEY_ALT)
            modifier_pressed[MODI_ALT]  = false;
        else if (sc == KEY_LSHIFT)
            modifier_pressed[MODI_LSHIFT] = false;
        else if (sc == KEY_RSHIFT)
            modifier_pressed[MODI_RSHIFT] = false;
    } else {
        // key pressed
        if (sc == KEY_CNTRL)
            modifier_pressed[MODI_CTRL] = true;
        else if (sc == KEY_ALT)
            modifier_pressed[MODI_ALT]  = true;
        else if (sc == KEY_LSHIFT)
            modifier_pressed[MODI_LSHIFT] = true;
        else if (sc == KEY_RSHIFT)
            modifier_pressed[MODI_RSHIFT] = true;
        else {
            if (modifier_pressed[MODI_LSHIFT] || modifier_pressed[MODI_RSHIFT])
                ch = shift_table[kbdus[sc]];
            else
                ch = kbdus[sc];
            console::put(ch);
        }
    }

    if (ch) {
        buffer[buffer_end] = ch;
        sw_barrier();
        buffer_end = (buffer_end + 1) % sizeof_array(buffer);
        sw_barrier();
        buffer_nonempty.wake();
    }

    // reset
    uint8_t val = inb(0x61);
    val |= 0x82;
    outb(0x61, val);
    val &= 0x7f;
    outb(0x61, val);
}

static void callback(isr::registers&)
{
    read_scancode();
}

char getch()
{
    while (buffer_begin == buffer_end)
        buffer_nonempty.wait();

    char ch = buffer[buffer_begin];
    sw_barrier();
    buffer_begin = (buffer_begin + 1) % sizeof_array(buffer);
    return ch;
}

void init()
{
    // initialize shift table
    for (int i=0;i<128;i++)
        shift_table[i] = (char)i;
    for (int i=0;i<26;i++)
        shift_table[i+'a'] = char(i+'A');
    shift_table['1'] = '!';
    shift_table['2'] = '@';
    shift_table['3'] = '#';
    shift_table['4'] = '$';
    shift_table['5'] = '%';
    shift_table['6'] = '^';
    shift_table['7'] = '&';
    shift_table['8'] = '*';
    shift_table['9'] = '(';
    shift_table['0'] = ')';
    shift_table['`'] = '~';
    shift_table['-'] = '_';
    shift_table['='] = '+';
    shift_table['['] = '{';
    shift_table[']'] = '}';
    shift_table['\\'] = '|';
    shift_table[';'] = ':';
    shift_table['\''] = '"';
    shift_table[','] = '<';
    shift_table['.'] = '>';
    shift_table['/'] = '?';

    isr::register_int_handler(isr::IRQ1, callback);
}

}
}
