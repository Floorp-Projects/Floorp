#!/bin/bash
set -e

# This script is for building GCC 7 for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

root_dir=$HOME_DIR
data_dir=$HOME_DIR/src/build/unix/build-gcc

. $data_dir/build-gcc.sh

gcc_version=7.3.0
gcc_ext=xz
binutils_version=2.28.1
binutils_ext=xz

$HOME_DIR/src/taskcluster/scripts/misc/fetch-content task-artifacts -d $root_dir $MOZ_FETCHES

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
cp $HOME_DIR/gcc.tar.* $UPLOAD_DIR
