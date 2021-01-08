#!/bin/bash
set -x -e -v

# This script is for building clang for Mac OS X targets on a Linux host,
# including native Mac OS X Compiler-RT libraries and llvm-symbolizer.

mkdir -p $UPLOAD_DIR
cd $MOZ_FETCHES_DIR

# We have a native linux64 toolchain in $MOZ_FETCHES_DIR/clang
# We have a native macosx64 toolchain in $MOZ_FETCHES_DIR/clang-mac/clang
# What we want is a mostly-linux64 toolchain but with a macosx64 llvm-symbolizer and runtime libraries

mv clang-mac/clang/bin/llvm-symbolizer clang/bin/
cp --remove-destination -lr clang/* clang-mac/clang/

cd clang-mac
tar -c clang | $GECKO_PATH/taskcluster/scripts/misc/zstdpy > $UPLOAD_DIR/clang.tar.zst
