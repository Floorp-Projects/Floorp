#!/bin/bash
set -e

# This script is for building a MinGW GCC (and headers) to be used on Linux to compile for Windows.

root_dir=$MOZ_FETCHES_DIR
data_dir=$GECKO_PATH/build/unix/build-gcc

. $data_dir/build-gcc.sh

binutils_configure_flags="--target=i686-w64-mingw32"
mingw_version=bcf1f29d6dc80b6025b416bef104d2314fa9be57

pushd $root_dir/gcc-source
ln -sf ../gmp-source gmp
ln -sf ../isl-source isl
ln -sf ../mpc-source mpc
ln -sf ../mpfr-source mpfr
popd

prepare_mingw
build_binutils
build_gcc_and_mingw

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp $root_dir/mingw32.tar.* $UPLOAD_DIR
