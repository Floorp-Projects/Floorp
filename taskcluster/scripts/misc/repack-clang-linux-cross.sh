#!/bin/bash
set -x -e -v

# This script is for building clang for cross Linux targets on a Linux host.

cd $MOZ_FETCHES_DIR

# We have a native linux64 toolchain in $MOZ_FETCHES_DIR/clang
# We have some linux compiler-rts in $MOZ_FETCHES_DIR/compiler-rt if no argument
# is given to the script, or $MOZ_FETCHES_DIR/*/compiler-rt otherwise. The
# subdirectories are given as arguments.
clang_lib=$(echo $PWD/clang/lib/clang/*/lib)
for dir in ${*:-.}; do
  find $dir/compiler-rt/lib/linux -type f | while read f; do
    cp -n $f $clang_lib/linux
  done
done

tar -caf clang.tar.zst clang

mkdir -p $UPLOAD_DIR
mv clang.tar.zst $UPLOAD_DIR
