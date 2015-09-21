/* Text terminal device header.
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

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

namespace console
{

// puts a charater to the screen
void put(char c);

// puts a string to the screen
void puts(const char* s);

int  printf(const char* fmt, ...);

// clears the screen
void clear();

}

#endif  /* _CONSOLE_H_ */
