#!/bin/bash
set -x -e -v

# This script is for building libc-wasi.

cd $GECKO_PATH

# gets a bit too verbose here

cd $MOZ_FETCHES_DIR/wasi-libc
make WASM_CC=$MOZ_FETCHES_DIR/clang/bin/clang \
     WASM_AR=$MOZ_FETCHES_DIR/clang/bin/llvm-ar \
     WASM_NM=$MOZ_FETCHES_DIR/clang/bin/llvm-nm \
     SYSROOT=$(pwd)/wasi-sysroot

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
tar caf $UPLOAD_DIR/wasi-sysroot.tar.xz wasi-sysroot
