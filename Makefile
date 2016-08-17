## tools
TARGET=i686-elf
CC=${TARGET}-gcc
CXX=${TARGET}-g++
ASM=yasm

export CC
export CXX
export ASM

all:
	$(MAKE) -C ./kernel

os.img:	all
	@echo "Writing files to disk image..."
	test -s os.img || ./scripts/mkimg.sh
	sudo mount -o loop,offset=1048576 ./os.img /mnt/loop
	sudo cp ./kernel/kernel /mnt/loop/boot/
	sudo umount /mnt/loop

.PHONY:	run rungdb runbochs
run: os.img
	-qemu-system-x86_64 -d cpu_reset -m 512 -drive file=os.img,if=none,id=disk -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0

rungdb: os.img
	-qemu-system-x86_64 -d cpu_reset -m 512 -s -S -drive file=os.img,if=none,id=disk -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0 & (sleep 0.1; gdb kernel/kernel -ex 'symbol-file kernel/kernel.sym' -ex 'target remote localhost:1234')

runbochs: os.img
	-bochs -f scripts/bochsrc


.PHONY:	clean cleandep
clean:
	$(MAKE) -C ./kernel clean

cleandep:
	$(MAKE) -C ./kernel cleandep
