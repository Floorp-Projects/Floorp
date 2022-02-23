#!/bin/bash
set -x -e -v

# This script is for building clang for macOS targets on a Linux host,
# including native macOS Compiler-RT libraries.

cd $MOZ_FETCHES_DIR

# We have a native linux64 toolchain in $MOZ_FETCHES_DIR/clang
# We have a native aarch64 macos compiler-rt in $MOZ_FETCHES_DIR/aarch64/compiler-rt
# We have a native x86_64 macos compiler-rt in $MOZ_FETCHES_DIR/x86_64/compiler-rt
clang_lib=$(echo clang/lib/clang/*/lib)
mkdir -p $clang_lib/darwin
find {aarch64,x86_64}/compiler-rt/lib/darwin -type f -printf '%f\n' | sort -u | while read f; do
  f=compiler-rt/lib/darwin/$f
  if [ -f aarch64/$f -a -f x86_64/$f ]; then
    # For compiler-rt files that exist on both ends, merge them
    $MOZ_FETCHES_DIR/cctools/bin/lipo -create {aarch64,x86_64}/$f -output $clang_lib/darwin/${f##*/}
  elif [ -f aarch64/$f ]; then
    # For compiler-rt files that exist on either end, copy the existing one
    cp aarch64/$f $clang_lib/darwin
  else
    cp x86_64/$f $clang_lib/darwin
  fi
done

tar caf clang.tar.zst clang

mkdir -p $UPLOAD_DIR
mv clang.tar.zst $UPLOAD_DIR
