#!/bin/bash
set -e

# This script is for building GCC 6 for Linux.

root_dir=$MOZ_FETCHES_DIR
data_dir=$GECKO_PATH/build/unix/build-gcc

. $data_dir/build-gcc.sh

gcc_version=6.4.0
gcc_ext=xz
binutils_version=2.31.1
binutils_ext=xz

pushd $root_dir/gcc-$gcc_version
ln -sf ../gmp-5.1.3 gmp
ln -sf ../isl-0.15 isl
ln -sf ../mpc-0.8.2 mpc
ln -sf ../mpfr-3.1.5 mpfr
popd

build_binutils
build_gcc

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $MOZ_FETCHES_DIR/gcc.tar.* $UPLOAD_DIR
