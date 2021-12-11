#!/bin/bash
set -e

# This script is for building GCC for Linux.

root_dir=$MOZ_FETCHES_DIR
data_dir=$GECKO_PATH/build/unix/build-gcc

PATH=$MOZ_FETCHES_DIR/gcc/bin:$PATH

. $data_dir/build-gcc.sh

pushd $root_dir/gcc-source
ln -sf ../gmp-source gmp
ln -sf ../isl-source isl
ln -sf ../mpc-source mpc
ln -sf ../mpfr-source mpfr
popd

build_gcc

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $MOZ_FETCHES_DIR/gcc.tar.* $UPLOAD_DIR
