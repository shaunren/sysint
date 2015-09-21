;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; x86 system call (via sysenter) entry point.
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

extern syscall_table
SYSCALL_COUNT equ 11

ENOSYS equ 88

;; syscall entry point
;; syscall id = eax
;; args = edi, esi, edx, ecx
global syscall_entry
align 16
syscall_entry:
        ;; crheck if the syscall id is valid
        cmp eax, SYSCALL_COUNT
        jge .ret_enosys

        ;; push args
        push ecx
        push edx
        push esi
        push edi

        mov cx, 0x10            ; kernel data segment descriptor
        mov ds, cx
        mov es, cx
        mov fs, cx
        mov gs, cx

        ;; dispatch
        push .ret
        jmp  [4*eax + syscall_table]

align 4                         ; cold path
.ret_enosys:
        mov eax, -ENOSYS
        mov ecx, ebp
        mov edx, ebx
        sti
        sysexit

align 16
.ret:
        cli
        add esp, 16

        mov cx, 0x20|0x03       ; user data segment, RPL 3
        mov ds, cx
        mov es, cx
        mov fs, cx
        mov gs, cx

        mov ecx, ebp            ; cdecl non-volatile registers
        mov edx, ebx
        sti
        sysexit
