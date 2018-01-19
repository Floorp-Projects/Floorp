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
git checkout 4727bc748a48e46824eae55a81ae890cd25c3a34

mkdir build
cd build

cmake \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_TARGETS_TO_BUILD=X86 \
  -DCMAKE_C_COMPILER=$HOME_DIR/src/gcc/bin/gcc \
  ..

export LD_LIBRARY_PATH=$HOME_DIR/src/gcc/lib64

ninja llvm-dsymutil

tar --xform='s,^,llvm-dsymutil/,' -Jcf llvm-dsymutil.tar.xz bin/llvm-dsymutil

mkdir -p $UPLOAD_DIR
cp llvm-dsymutil.tar.xz $UPLOAD_DIR
