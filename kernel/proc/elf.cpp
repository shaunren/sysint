/* ELF executable loader.
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

#include "elf.h"
#include <paging.h>
#include <memory.h>
#include <lib/string.h>
#include <errno.h>
#include <algorithm>


using namespace paging;
using std::max;

namespace elf
{

bool Elf32_Ehdr::valid() const
{
    if (memcmp(e_ident, ELFMAG, SELFMAG) ||
        e_ident[EI_CLASS] != ELFCLASS32 ||
        e_ident[EI_DATA] != ELFDATA2LSB ||
        e_machine != EM_386 ||
        e_ident[EI_VERSION] != EV_CURRENT ||
        e_ident[EI_OSABI] != ELFOSABI_NONE ||
        (e_type != ET_REL && e_type != ET_EXEC)) {
        return false;
    }
    return true;
}

const char* Elf32_Ehdr::strtab() const
{
    if (e_shstrndx == SHN_UNDEF) return nullptr;
    return (const char*)this + (get_shdr() + e_shstrndx)->sh_offset;
}

const char* Elf32_Ehdr::get_str(size_t offset) const
{
    auto strtab = this->strtab();
    return !strtab ? nullptr : strtab + offset;
}

// dir must contain the current kernel stack and buf
// this function will load dir in the process
int load(const void* buf, page_dir* dir, Elf32_Addr& entry, void*& brk_start)
{
    if (unlikely(!buf || !dir))
        return -EFAULT;

    const Elf32_Ehdr* hdr = (const Elf32_Ehdr*) buf;
    if (!hdr->valid())
        return -ENOEXEC;

    auto phdr = hdr->get_phdr();
    if (unlikely(!phdr))
        return -ENOEXEC;

    brk_start = (void*) 0;

    for (unsigned i=0; i < hdr->e_phnum; ++i, ++phdr) {
        if (phdr->p_type == PT_PHDR)
            continue; // ignore
        if (phdr->p_type == PT_LOAD) {
            const auto vaddr_start = phdr->p_vaddr & ~(PAGE_SIZE - 1);
            const auto vaddr_end = memory::align_addr(phdr->p_vaddr +
                                                      phdr->p_memsz);
            if (!dir->alloc_block((void*)vaddr_start,
                                 vaddr_end - vaddr_start,
                                 PAGE_PRESENT|PAGE_US|PAGE_RW))
                return -ENOMEM;
            brk_start = (void*) max((uintptr_t)brk_start, vaddr_end);
        } else
            return -EINVAL;
    }

    phdr = hdr->get_phdr();

    paging::switch_page_dir(dir);

     // copy over the contents
    for (unsigned i=0; i < hdr->e_phnum; ++i, ++phdr) {
        if (phdr->p_type == PT_PHDR)
            continue; // ignore

        if (unlikely(phdr->p_filesz & 3))
            memcpy((void*)phdr->p_vaddr, (char*)buf + phdr->p_offset, phdr->p_filesz);
        else
            memcpyd((void*)phdr->p_vaddr, (char*)buf + phdr->p_offset, phdr->p_filesz/4);
        // clear extra memory to 0
        if (phdr->p_memsz > phdr->p_filesz) {
            const auto sz = phdr->p_memsz - phdr->p_filesz;
            if (unlikely(sz & 3))
                memset((char*)phdr->p_vaddr + phdr->p_filesz, 0, sz);
            else
                memsetd((char*)phdr->p_vaddr + phdr->p_filesz, 0, sz/4);
        }

        if (phdr->p_align >= PAGE_SIZE && !(phdr->p_flags & PF_W)) {
            // set pages as read-only if we are able to
            const auto vaddr_start = phdr->p_vaddr & ~(PAGE_SIZE - 1);
            const auto vaddr_end = memory::align_addr(phdr->p_vaddr +
                                                      phdr->p_memsz);
            for (auto addr = vaddr_start; addr < vaddr_end; addr += PAGE_SIZE)
                dir->get_page((void*)addr)->rw = false;
        }
    }

    entry = hdr->e_entry;

    return 0;
}

}
