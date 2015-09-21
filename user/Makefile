CXX = i686-elf-g++
CC  = i686-elf-gcc

## flags
CXXFLAGS = -std=gnu++14 -masm=intel -g3 -O2 -MMD -I../include -Wall -ffreestanding -fno-exceptions -fno-rtti
CFLAGS  = -masm=intel -O2 -MMD -I../include -Wall -ffreestanding
LDFLAGS = -ffreestanding -nostdlib
ASFLAGS = -felf

.DEFAULT_GOAL := all

CRTI_OBJ = ../kernel/lib/crti.o
CRTBEGIN_OBJ := $(shell $(CXX) $(CXXFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ := $(shell $(CXX) $(CXXFLAGS) -print-file-name=crtend.o)
CRTN_OBJ = ../kernel/lib/crtn.o
CRT0_OBJ = crt0.o

COMMON_OBJ = common.o

BINS = test1 test2

## include dependencies
DEPS := $(OBJS:.o=.d)
-include $(DEPS)

all: $(BINS)

$(BINS): %: %.o $(CRT0_OBJ) $(COMMON_OBJ)
	$(CXX) $(LDFLAGS) $(CRTI_OBJ) $(CRTBEGIN_OBJ) $(CRT0_OBJ) $< $(CRTEND_OBJ) $(CRTN_OBJ) $(COMMON_OBJ) -o $@
	strip $@

.s.o:
	$(ASM) $(ASFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f $(BINS) *.o *.d
