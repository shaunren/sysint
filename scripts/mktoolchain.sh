#!/bin/bash
## Make a cross compiling toolchain with C++ support.
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

export PREFIX=${PREFIX:-/usr/local/cross}
export TARGET=${TARGET:-i686-elf}
export PATH="$PREFIX/bin:$PATH"

BINUTILS=binutils-2.27
GCC=gcc-6.1.0
NEWLIB=newlib-2.4.0.20160527

echo "Downloading source tarballs..."
wget "https://ftp.gnu.org/gnu/binutils/$BINUTILS.tar.gz"
wget "https://ftp.gnu.org/gnu/gcc/$GCC/$GCC.tar.gz"
wget "ftp://sourceware.org/pub/newlib/$NEWLIB.tar.gz"

mkdir -p binutils/build gcc/build newlib/build
tar xzf "$BINUTILS.tar.gz" -C binutils --strip-components=1
tar xzf "$GCC.tar.gz" -C gcc --strip-components=1
tar xzf "$NEWLIB.tar.gz" -C newlib --strip-components=1

ncores=`grep -c '^processor' /proc/cpuinfo`

cd binutils/build
echo
echo "Building binutils..."
../configure --target="$TARGET" --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j"$ncores"
sudo make install
cd ..
rm -rf build


cd ../gcc/build
echo
echo "Building gcc (pass 1)..."
../configure --target="$TARGET" --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers --disable-hosted-libstdcxx
make -j"$ncores" all-gcc all-target-libgcc
sudo make install-gcc install-target-libgcc

cd ../../newlib/build
echo
echo "Building newlib..."
../configure --target="$TARGET" --prefix="$PREFIX"
make -j"$ncores" all
sudo make install
cd ..
rm -rf build

cd ../gcc/build
echo
echo "Building gcc (pass 2)..."
../configure --target="$TARGET" --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --with-newlib
make -j"$ncores" all-target-libstdc++-v3
sudo make install-target-libstdc++-v3
cd ..
rm -rf build
cd ..

echo
echo "Done."
