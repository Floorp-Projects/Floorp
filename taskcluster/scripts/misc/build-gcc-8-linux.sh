#!/bin/bash
set -e

# This script is for building GCC 7 for Linux.

root_dir=$MOZ_FETCHES_DIR
data_dir=$GECKO_PATH/build/unix/build-gcc

. $data_dir/build-gcc.sh

gcc_version=8.3.0
gcc_ext=xz
binutils_version=2.31.1
binutils_ext=xz

pushd $root_dir/gcc-$gcc_version
ln -sf ../gmp-6.1.0 gmp
ln -sf ../isl-0.16.1 isl
ln -sf ../mpc-1.0.3 mpc
ln -sf ../mpfr-3.1.4 mpfr
popd

build_binutils
build_gcc

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $MOZ_FETCHES_DIR/gcc.tar.* $UPLOAD_DIR
