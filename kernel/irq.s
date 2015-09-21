;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; x86 CPU interrupt entry point.
;; Copyright (C) 2014,2015 Shaun Ren.
;;
;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

;; saves CPU state, sets up for kernel mode segments, calls C++ fault
;; handler, and restores the stack frame.

;; saves CPU state and set up kernel mode segments
%macro SET_KERNEL_STATE 0
        pusha

        mov bx, ds              ; ebx is non-volatile in cdecl

        mov ax, 0x10            ; kernel data segment descriptor
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
%endmacro

;; restore original CPU state
%macro RESTORE_CPU_STATE 0
        mov ds, bx
        mov es, bx
        mov fs, bx
        mov gs, bx

        popa
        add esp, 8
        iret
%endmacro

extern isr_handler

%macro ISR_COMMON_STUB 0
        SET_KERNEL_STATE
        call isr_handler
        RESTORE_CPU_STATE
%endmacro  ; ISR_COMMON_STUB

;; ISR handlers
%macro ISR_NOERR 1
global isr%1
align 16
isr%1:
        push byte 0
        push byte %1
        ISR_COMMON_STUB
%endmacro  ; ISR_NOERR

%macro ISR_ERR 1
global isr%1
align 16
isr%1:
        push byte %1
        ISR_COMMON_STUB
%endmacro  ; ISR_ERR


ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_NOERR 17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31


extern irq_handler
%macro IRQ 2
global irq%1
align 16
irq%1:
        push byte 0
        push byte %2

        SET_KERNEL_STATE
        call irq_handler
        RESTORE_CPU_STATE
%endmacro  ; IRQ

IRQ  0,  32
IRQ  1,  33
IRQ  2,  34
IRQ  3,  35
IRQ  4,  36
IRQ  5,  37
IRQ  6,  38
IRQ  7,  39
IRQ  8,  40
IRQ  9,  41
IRQ 10,  42
IRQ 11,  43
IRQ 12,  44
IRQ 13,  45
IRQ 14,  46
IRQ 15,  47
IRQ 16,  48
