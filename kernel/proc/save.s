;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; x86 CPU state preserving functions.
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

;; save current state; eax is not saved faithfully
;; note that esp may be incorrect; set it manually after
;; void save_state(proc_state* state)
global save_state
align 16
save_state:
        lea eax, [esp+8]        ; save esp (+ 8: exclude stack frame)
        mov esp, [esp+4]        ; proc_state

        add esp, 40
        pusha

        mov [esp+12], eax       ; esp, excluding current stack frame

        sub esp, 4
        mov ecx, esp            ; proc_state eip

        pushf

        lea esp, [eax-8]        ; restore old esp

        mov eax, [esp]
        mov [ecx], eax          ; eip

        ret

;; same as save_state, additionally unfaithful to eip and esp
;; this is to be used with __schedule
;; void save_state_no_eip_esp(proc_state* state)
global save_state_no_eip_esp
align 16
save_state_no_eip_esp:
        mov eax, esp            ; save esp
        mov esp, [esp+4]        ; proc_state

        add esp, 40
        pusha

        sub esp, 4
        pushf

        mov esp, eax            ; restore old esp
        ret
