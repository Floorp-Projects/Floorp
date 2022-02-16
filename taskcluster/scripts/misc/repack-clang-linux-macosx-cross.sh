#!/bin/bash
set -x -e -v

# This script is for building clang for macOS targets on a Linux host,
# including native macOS Compiler-RT libraries.

cd $MOZ_FETCHES_DIR

# We have a native linux64 toolchain in $MOZ_FETCHES_DIR/clang
# We have a native aarch64 macos compiler-rt in $MOZ_FETCHES_DIR/compiler-rt-aarch64-apple-darwin
# We have a native x86_64 macos compiler-rt in $MOZ_FETCHES_DIR/compiler-rt-x86_64-apple-darwin
clang_lib=$(echo clang/lib/clang/*/lib)
mkdir -p $clang_lib/darwin
find compiler-rt-*/lib/darwin -type f -printf '%f\n' | sort -u | while read f; do
  f=lib/darwin/$f
  if [ -f compiler-rt-aarch64-apple-darwin/$f -a -f compiler-rt-x86_64-apple-darwin/$f ]; then
    # For compiler-rt files that exist on both ends, merge them
    $MOZ_FETCHES_DIR/cctools/bin/lipo -create compiler-rt-{aarch64,x86_64}-apple-darwin/$f -output $clang_lib/darwin/${f##*/}
  elif [ -f compiler-rt-aarch64-apple-darwin/$f ]; then
    # For compiler-rt files that exist on either end, copy the existing one
    cp compiler-rt-aarch64-apple-darwin/$f $clang_lib/darwin
  else
    cp compiler-rt-x86_64-apple-darwin/$f $clang_lib/darwin
  fi
done

tar caf clang.tar.zst clang

mkdir -p $UPLOAD_DIR
mv clang.tar.zst $UPLOAD_DIR
