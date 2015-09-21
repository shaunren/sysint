## flags
CXXFLAGS = -std=gnu++14 -masm=intel -g3 -O2 -MMD -I../include -I./include   \
           -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter \
           -ffreestanding -fstack-protector-strong -fno-exceptions -fno-rtti
LDFLAGS = -Tkernel.ld -ffreestanding -nostdlib
ASFLAGS = -felf

.DEFAULT_GOAL := kernel

CRTI_OBJ = lib/crti.o
CRTBEGIN_OBJ := $(shell $(CXX) $(CXXFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ := $(shell $(CXX) $(CXXFLAGS) -print-file-name=crtend.o)
CRTN_OBJ = lib/crtn.o

OBJS = boot.o kmain.o gdtflush.o gdt.o irq.o idt.o isr.o time.o

include lib/rules.mk
include mem/rules.mk
include proc/rules.mk
include fs/rules.mk
include devices/rules.mk
include syscall/rules.mk

## include dependencies
DEPS := $(OBJS:.o=.d)
-include $(DEPS)

kernel:	$(OBJS) $(CRTI_OBJ) $(CRTN_OBJ) kernel.ld
	$(CXX) $(LDFLAGS) -o kernel $(CRTI_OBJ) $(CRTBEGIN_OBJ) $(OBJS) $(CRTEND_OBJ) $(CRTN_OBJ) -lgcc
	objcopy --only-keep-debug kernel kernel.sym
	strip kernel

.s.o:
	$(ASM) $(ASFLAGS) $< -o $@


.PHONY: clean cleandep
clean:
	-rm -f kernel kernel.sym
	-find . -type f \( -name '*.o' -o -name '*.d' \) -exec rm -f {} \;

cleandep:
	-find . -type f -name '*.d' -exec rm -f {} \;
