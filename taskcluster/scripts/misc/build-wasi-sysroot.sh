#!/bin/bash
set -x -e -v

export PATH="$MOZ_FETCHES_DIR/gcc/bin:$MOZ_FETCHES_DIR/binutils/bin:$PATH"
export LD_LIBRARY_PATH="$MOZ_FETCHES_DIR/gcc/lib64/:$LD_LIBRARY_PATH"
cd $MOZ_FETCHES_DIR/wasi-sdk
PREFIX=$MOZ_FETCHES_DIR/wasi-sdk/wasi-sysroot
make PREFIX=$PREFIX
# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
tar -C $MOZ_FETCHES_DIR/wasi-sdk/ -caf $UPLOAD_DIR/wasi-sysroot.tar.xz wasi-sysroot
