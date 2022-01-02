#!/bin/bash
set -x -e -v

# This script is creating an artifact containing llvm-symbolizer.

mkdir llvm-symbolizer
cp $MOZ_FETCHES_DIR/clang/bin/llvm-symbolizer* llvm-symbolizer/

tar caf llvm-symbolizer.tar.zst llvm-symbolizer

mkdir -p $UPLOAD_DIR
mv llvm-symbolizer.tar.zst $UPLOAD_DIR
