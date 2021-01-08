#!/bin/bash
set -x -e -v

# This script is for building clang for macos targets on a Linux host,
# including native aarch64 macos Compiler-RT libraries.

cd $MOZ_FETCHES_DIR

# We have a native linux64 toolchain in $MOZ_FETCHES_DIR/clang
# We have a native aarch64 macos compiler-rt in $MOZ_FETCHES_DIR/compiler-rt
cp -r compiler-rt/lib/darwin clang/lib/clang/*/lib

tar -cf - clang | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > clang.tar.zst

mkdir -p $UPLOAD_DIR
mv clang.tar.zst $UPLOAD_DIR
