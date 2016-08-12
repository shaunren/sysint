#!/bin/bash
## Make a bootable sysint image with an ext2 partition.
## Copyright (C) 2014 Shaun Ren.
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 3 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.

set -e
echo 'Creating a new image...'
qemu-img create os.img ${1:-512m}
outlo=`sudo losetup -f --show os.img`
sudo parted $outlo mklabel msdos
sudo parted $outlo mkpart primary ext2 1m 90%
sudo parted $outlo set 1 boot on
! sudo partx -a $outlo
sudo mke2fs ${outlo}p1
sudo mkdir -p /mnt/loop
sudo mount -t ext2 ${outlo}p1 /mnt/loop
sudo grub-install --target=i386-pc --boot-directory=/mnt/loop/boot/ --modules="ext2 part_msdos" $outlo
sudo cp -R ./root/* /mnt/loop/
sudo umount /mnt/loop
sudo partx -d ${outlo}p1
sudo losetup -d $outlo
