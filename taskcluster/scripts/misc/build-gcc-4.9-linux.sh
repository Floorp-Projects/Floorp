#!/bin/bash
set -e

# This script is for building GCC 4.9 for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

root_dir=$HOME_DIR
data_dir=$HOME_DIR/src/build/unix/build-gcc

. $data_dir/build-gcc.sh

gcc_version=4.9.4
gcc_ext=bz2
binutils_version=2.25.1
binutils_ext=bz2

pushd $root_dir/gcc-$gcc_version
ln -sf ../cloog-0.18.1 cloog
ln -sf ../gmp-5.1.3 gmp
ln -sf ../mpc-0.8.2 mpc
ln -sf ../isl-0.12.2 isl
ln -sf ../mpfr-3.1.5 mpfr
popd

apply_patch $data_dir/PR64905.patch
build_binutils
build_gcc

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $HOME_DIR/gcc.tar.* $UPLOAD_DIR
