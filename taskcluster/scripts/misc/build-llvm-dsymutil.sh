#!/bin/bash
set -x -e -v

# This script is for building clang for Linux.

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

cd $MOZ_FETCHES_DIR/llvm-project/llvm

mkdir build
cd build

cmake \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD=X86 \
  -DCMAKE_C_COMPILER=$MOZ_FETCHES_DIR/gcc/bin/gcc \
  ..

export LD_LIBRARY_PATH=$MOZ_FETCHES_DIR/gcc/lib64

ninja dsymutil llvm-symbolizer

tar --xform='s,^,llvm-dsymutil/,' -Jcf llvm-dsymutil.tar.xz bin/dsymutil bin/llvm-symbolizer

mkdir -p $UPLOAD_DIR
cp llvm-dsymutil.tar.xz $UPLOAD_DIR
