#!/bin/bash
set -e

# This script is for building a MinGW GCC (and headers) to be used on Linux to compile for Windows.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

# Do not define root_dir so we build everything to a tmpdir
#root_dir=$HOME_DIR
data_dir=$HOME_DIR/src/build/unix/build-gcc

. $data_dir/build-gcc.sh

gcc_version=6.4.0
gcc_ext=xz
binutils_version=2.27
binutils_ext=bz2
binutils_configure_flags="--target=i686-w64-mingw32"
mingw_version=bcf1f29d6dc80b6025b416bef104d2314fa9be57

pushd $root_dir/gcc-$gcc_version
ln -sf ../gmp-5.1.3 gmp
ln -sf ../isl-0.15 isl
ln -sf ../mpc-0.8.2 mpc
ln -sf ../mpfr-3.1.5 mpfr
popd

prepare_mingw
build_binutils
build_gcc_and_mingw

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $root_dir/mingw32.tar.* $UPLOAD_DIR
