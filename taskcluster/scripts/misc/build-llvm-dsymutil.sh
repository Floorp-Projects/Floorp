#!/bin/bash
set -x -e -v

# This script is for building clang for Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

cd $HOME_DIR/src

. taskcluster/scripts/misc/tooltool-download.sh

git clone -n https://github.com/llvm-mirror/llvm

cd llvm
git checkout 9df0977d9409b093156ebbd8e1ac99bc32b9eb39

mkdir build
cd build

cmake \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD=X86 \
  -DCMAKE_C_COMPILER=$HOME_DIR/src/gcc/bin/gcc \
  ..

export LD_LIBRARY_PATH=$HOME_DIR/src/gcc/lib64

ninja dsymutil llvm-symbolizer

tar --xform='s,^,llvm-dsymutil/,' -Jcf llvm-dsymutil.tar.xz bin/dsymutil bin/llvm-symbolizer

mkdir -p $UPLOAD_DIR
cp llvm-dsymutil.tar.xz $UPLOAD_DIR
