;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; x86 task switch functions.
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

%macro set_user_datasegs 0
        mov ax, 0x20|0x03       ; user data segment, RPL 3
        mov ds, ax
        mov es, ax
        mov fs, ax
        mov gs, ax
%endmacro

%macro set_user_flags_cs 0
        pushf
        pop eax
        or eax, 0x200          ; set IF flag
        push eax                   ; EFLAGS

        push 0x18|0x03             ; CS
%endmacro

global switch_to_user_curreg
align 16
switch_to_user_curreg:
        cli

        set_user_datasegs

        mov eax, esp
        push 0x20|0x03             ; SS

        push eax                   ; ESP

        set_user_flags_cs

        push .switch_to_user_curreg_end   ; EIP

        iret                              ;; I!

.switch_to_user_curreg_end:
        ret

;; void switch_proc(page_dir* dir, proc_state state)
global switch_proc
align 16
switch_proc:
        cli
        add esp, 16
        popa

        mov eax, [esp-44]
        mov cr3, eax            ; page directory

        mov ecx, esp
        mov esp, [esp-20]       ; new esp

        ;; hopefully the kernel stack still has 8 bytes available
        mov eax, [ecx-36]
        push eax                ;; EIP

        mov eax, [ecx-40]
        push eax                ; EFLAGS

        mov eax, [ecx-4]        ; original eax
        mov ecx, [ecx-8]        ; original ecx

        popf                    ;; EFLAGS

        ret


;; void switch_proc_user(page_dir* dir, proc_state state)
global switch_proc_user
align 16
switch_proc_user:
        cli
        add esp, 16
        popa
        push eax                ; save original eax

        push 0x20|0x03          ;; SS
        mov eax, [esp-12]
        push eax                ;; ESP
        mov eax, [esp-28]
        push eax                ;; EFLAGS
        push 0x18|0x03          ;; CS
        mov eax, [esp-16]
        push eax                ;; EIP

        mov eax, [esp-20]
        mov cr3, eax            ; page directory

        set_user_datasegs

        mov eax, [esp+20]       ; original eax
        iret
