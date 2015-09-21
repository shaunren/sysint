;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Process scheduler helper that switches to kstack before rescheduling.
;; Copyright (C) 2015 Shaun Ren.
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

extern _kstack_top
extern _kernel_end

extern schedule
extern save_state_no_eip_esp

global __schedule_switch_kstack_and_call
align 16
__schedule_switch_kstack_and_call:
        ;; check if we are using a shared kernel stack alreday
        cmp  esp, _kernel_end
        jl   .sched

        ;; switch to kstack
        mov  esp, _kstack_top

.sched:
        push 0
        call schedule
        ret 4

global __schedule_switch_kstack_and_call_save
align 16
__schedule_switch_kstack_and_call_save:
        mov ecx, [esp+4]        ; proc_state*

        push ecx
        call save_state_no_eip_esp
        pop  ecx

        mov eax, [esp]    ; eip
        mov [ecx+4], eax

        lea eax, [esp+4]  ; exclude return eip
        mov [ecx+20], eax ; esp

        ;; check if we are using a shared kernel stack alreday
        cmp  esp, _kernel_end
        jl   .sched

        ;; switch to kstack
        mov  esp, _kstack_top

.sched:
        push 0
        call schedule
        ret 4
