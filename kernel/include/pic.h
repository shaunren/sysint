/* x86 PIC controller codes.
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

#ifndef _PIC_H_
#define _PIC_H_

#define PIC_MASTER_CMD    0x20  /* PIC Master Command Port */
#define PIC_MASTER_DATA   0x21  /* PIC Master Data Port */
#define PIC_SLAVE_CMD     0xA0  /* PIC Slave Command Port */
#define PIC_SLAVE_DATA    0xA1  /* PIC Salve Data Port */

#define PIC_EOI           0x20  /* EOI (End-of-interrupt) command code */

#define ICW1_ICW4	  0x01	/* ICW4 (not) needed */
#define ICW1_SINGLE	  0x02	/* Single (cascade) mode */
#define ICW1_INTERVAL4	  0x04	/* Call address interval 4 (8) */
#define ICW1_LEVEL	  0x08	/* Level triggered (edge) mode */
#define ICW1_INIT	  0x10	/* Initialization - required! */
 
#define ICW4_8086	  0x01	/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO	  0x02	/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	  0x08	/* Buffered mode/slave */
#define ICW4_BUF_MASTER	  0x0C	/* Buffered mode/master */
#define ICW4_SFNM	  0x10	/* Special fully nested (not) */


#endif /* _PIC_H_ */
