#!/bin/bash
set -x -e -v

# This script is creating an artifact containing llvm-symbolizer.

mkdir llvm-symbolizer
cp $MOZ_FETCHES_DIR/clang/bin/llvm-symbolizer* llvm-symbolizer/

tar -cf - llvm-symbolizer | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > llvm-symbolizer.tar.zst

mkdir -p $UPLOAD_DIR
mv llvm-symbolizer.tar.zst $UPLOAD_DIR
