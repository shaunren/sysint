;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Kernel multiboot entry point.
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

PAGE_ALIGN        equ 1<<0
MEM_INFO          equ 1<<1
HEADER_MAGIC      equ 0x1BADB002

HEADER_FLAGS      equ PAGE_ALIGN | MEM_INFO
CHECKSUM          equ -(HEADER_MAGIC + HEADER_FLAGS)


KERNEL_VIRTUAL_BASE equ 0xC0000000                  ; start of kernel space
KERNEL_PAGE_NUMBER  equ (KERNEL_VIRTUAL_BASE >> 22)

bits 32

section .multiboot
global multiboot
align 4
multiboot:  ; multiboot header
        dd HEADER_MAGIC
        dd HEADER_FLAGS
        dd CHECKSUM

section .data
align 0x1000
global kernel_page_dir
kernel_page_dir:
    ; this page directory entry identity-maps the first 4 MiB of the 32-bit physical address space.
    ; it also maps the first 4 MiB to 0xC0000000
    dd 0x83
    times (KERNEL_PAGE_NUMBER - 1) dd 0                 ; Pages before kernel space.
    dd 0x83
    times (1024 - KERNEL_PAGE_NUMBER - 1) dd 0          ; Pages after the kernel image.
    times 1024 dd 0                                     ; table addresses
    dd (kernel_page_dir - KERNEL_VIRTUAL_BASE)          ; physical address of the page
        
section .text
global _start
extern kmain
extern _init
_start:
        cli

        ;; Setup paging for higher half kernel
        mov edx, (kernel_page_dir - KERNEL_VIRTUAL_BASE)
        mov cr3, edx

        mov edx, cr4
        or  edx, 0x00000610     ; enable 4 MiB pages and OSFXSR & OSXMMEXCPT(SSE)
        mov cr4, edx

        mov edx, cr0
        and edx, 0xFFFB         ; disable EM bit (for SSE)
        or  edx, 0x80000002     ; enable paging and MP (SSE)
        mov cr0, edx

        fninit                  ; initialize FPU

        lea edx, [.higherhalf]
        jmp edx

.higherhalf:
        ;; unmap the identity-mapped first 4MiB
        mov dword [kernel_page_dir], 0
        invlpg [0]

        mov  esp, _kstack_top      ; set up stack

        ;; push multiboot header location
        add  ebx, KERNEL_VIRTUAL_BASE
        push ebx

        push esi
        push edi
        call _init
        pop  edi
        pop  esi

        call kmain

.halt:  cli
        hlt
        jmp .halt

section .bss

global _kstack_bot
global _kstack_top
align 32
_kstack_bot:
        resb 8192        ; 8 KiB of stack
_kstack_top:

global _irq_stack_bot
global _irq_stack_top
align 32
_irq_stack_bot:
        resb 8192
_irq_stack_top:
