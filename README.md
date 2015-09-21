sysint
======

A UNIX-like OS. Currently only a kernel for i686 PCs.

## Features
* Written in mostly C++
* Works on i686
* Uses buddy allocator for page frame management
* A fair process scheduler like Linux's CFS
* Linux-like clone system call
* VFS
    * I/O to terminal via standard file descriptors
* Basic POSIX signal support
* ELF binary loading
