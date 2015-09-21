/* Text termial device.
   Copyright (C) 2014 Shaun Ren.

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
#include <multiboot.h>
#include <lib/klib.h>
#include <lib/string.h>
#include <stdint.h>

namespace console
{

static uint8_t cursor_x, cursor_y;
static uint16_t* video_mem = (uint16_t*) (0xB8000 + KERNEL_VIRTUAL_BASE); // video memory location

const uint8_t  attbyte = (0<<4) | (15&0x0F);
const uint16_t blank   = 0x20 | (15<<8);

// moves the cursor to the current specified location
static inline void move_cursor(int y, int x)
{
    uint16_t index = y*80 + x;

    outb(0x3D4, 14);
    outb(0x3D5, index>>8);
    outb(0x3D4, 15);
    outb(0x3D5, index);
}

// scrolls the text one line upwards if necessary
static inline void scroll()
{
    if (cursor_y >= 25) {
        memmoved(video_mem, video_mem+80, 80*24/2);
        memsetw(video_mem+80*24, blank, 80); // fill the last row with blank

        cursor_y = 24;
    }
}

void put(char c)
{
    uint16_t attribute = attbyte << 8;

    switch (c) {
    case '\b': // backspace
        if (cursor_x)
            cursor_x--;
        break;

    case '\t': // tab
        cursor_x = (cursor_x+8) & ~7; // align at a 8-byte boundary
        break;

    case '\n': // LF
        cursor_y++;
        // falls through to reset cursor_x
    case '\r': // CR
        cursor_x = 0;
        break;

    default:
        if (c >= ' ') {
            *(video_mem + (cursor_y*80+cursor_x)) = c | attribute;
            cursor_x++;
        }
        break;
    }

    if (cursor_x >= 80) { // start a new line
        cursor_x = 0;
        cursor_y++;
    }

    scroll();
    move_cursor(cursor_y, cursor_x);
}

void puts(const char* c)
{
    while (*c) put(*c++);
}

// clears the screen
void clear()
{
    memsetw(video_mem, blank, 80*25);
    // reset cursor location
    cursor_x = cursor_y = 0;
    move_cursor(cursor_y, cursor_x);
}

}
