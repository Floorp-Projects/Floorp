#!/bin/bash
set -e

# This script is for building GCC 6 for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

root_dir=$HOME_DIR
data_dir=$HOME_DIR/src/build/unix/build-gcc

. $data_dir/build-gcc.sh

gcc_version=6.4.0
gcc_ext=xz
binutils_version=2.28.1
binutils_ext=xz

$HOME_DIR/src/taskcluster/scripts/misc/fetch-content task-artifacts -d $root_dir $MOZ_FETCHES

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
cp $HOME_DIR/gcc.tar.* $UPLOAD_DIR
