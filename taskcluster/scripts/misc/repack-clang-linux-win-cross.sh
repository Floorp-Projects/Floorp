#!/bin/bash
set -x -e -v

# This script is for building clang for windows targets on a Linux host,
# including native Windows Compiler-RT libraries.

cd $MOZ_FETCHES_DIR

# We have a native linux64 toolchain in $MOZ_FETCHES_DIR/clang
# We have a native x86 windows compiler-rt in $MOZ_FETCHES_DIR/x86/compiler-rt
# We have a native x86_64 windows compiler-rt in $MOZ_FETCHES_DIR/x86_64/compiler-rt
clang_lib=$(echo clang/lib/clang/*/lib)
mkdir -p $clang_lib/windows
cp x86/compiler-rt/lib/windows/* $clang_lib/windows
cp x86_64/compiler-rt/lib/windows/* $clang_lib/windows

tar caf clang.tar.zst clang

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.zst $UPLOAD_DIR
