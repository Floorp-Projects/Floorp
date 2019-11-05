#!/bin/bash
set -x -e -v

cd $MOZ_FETCHES_DIR/lucet_sandbox_compiler

export PATH=$MOZ_FETCHES_DIR/binutils/bin:$MOZ_FETCHES_DIR/clang/bin:$PATH:$MOZ_FETCHES_DIR/rustc/bin:$MOZ_FETCHES_DIR/cmake/bin
export CC=$MOZ_FETCHES_DIR/clang/bin/clang
export CFLAGS="-L$MOZ_FETCHES_DIR/clang/lib"
export CXXFLAGS=$CFLAGS
export CXX=$MOZ_FETCHES_DIR/clang/bin/clang++
export AR=$MOZ_FETCHES_DIR/clang/bin/llvm-ar
export RUSTFLAGS="-C linker=$CXX -C link-arg=$CXXFLAGS"

make build
mkdir -p $UPLOAD_DIR
tar --xz -cf $UPLOAD_DIR/lucetc.tar.xz target/release/lucetc
