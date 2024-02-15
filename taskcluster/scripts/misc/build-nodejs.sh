#!/bin/bash
set -x -e -v

artifact=$(basename "$TOOLCHAIN_ARTIFACT")
project=${artifact%.tar.*}
workspace=$HOME/workspace

cd $MOZ_FETCHES_DIR/$project

gcc_major=8
export CFLAGS=--sysroot=$MOZ_FETCHES_DIR/sysroot
export CXXFLAGS"=--sysroot=$MOZ_FETCHES_DIR/sysroot  -isystem $MOZ_FETCHES_DIR/sysroot/usr/include/c++/$gcc_major -isystem $MOZ_FETCHES_DIR/sysroot/usr/include/x86_64-linux-gnu/c++/$gcc_major"
export LDFLAGS="--sysroot=$MOZ_FETCHES_DIR/sysroot -L$MOZ_FETCHES_DIR/sysroot/lib/x86_64-linux-gnu -L$MOZ_FETCHES_DIR/sysroot/usr/lib/x86_64-linux-gnu -L$MOZ_FETCHES_DIR/sysroot/usr/lib/gcc/x86_64-linux-gnu/$gcc_major"
export CC=$MOZ_FETCHES_DIR/gcc/bin/gcc
export CXX=$MOZ_FETCHES_DIR/gcc/bin/g++

./configure --verbose --prefix=/
make -j$(nproc) install DESTDIR=$workspace/$project

tar -C $workspace -acvf $artifact $project
mkdir -p $UPLOAD_DIR
mv $artifact $UPLOAD_DIR
