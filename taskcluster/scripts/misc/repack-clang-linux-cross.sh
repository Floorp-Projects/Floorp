#!/bin/bash
set -x -e -v

# This script is for building clang for cross Linux targets on a Linux host.

cd $MOZ_FETCHES_DIR

# We have a native linux64 toolchain in $MOZ_FETCHES_DIR/clang
# We have some linux compiler-rt in $MOZ_FETCHES_DIR/compiler-rt
clang_lib=$(echo clang/lib/clang/*/lib)
find compiler-rt/lib/linux -type f | while read f; do
  cp $f $clang_lib/linux
done

tar -caf clang.tar.zst clang

mkdir -p $UPLOAD_DIR
mv clang.tar.zst $UPLOAD_DIR
