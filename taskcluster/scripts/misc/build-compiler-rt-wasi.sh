#!/bin/bash
set -x -e -v

artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}

if [ -n "$1" ]; then
  patch -d $MOZ_FETCHES_DIR/wasi-sdk -p1 < $(dirname $0)/$1
fi

cd $MOZ_FETCHES_DIR/wasi-sdk
LLVM_PROJ_DIR=$MOZ_FETCHES_DIR/llvm-project

mkdir -p build/install/wasi
# The wasi-sdk build system wants to build clang itself. We trick it into
# thinking it did, and put our own clang where it would have built its own.
ln -s $MOZ_FETCHES_DIR/clang build/llvm
touch build/llvm.BUILT

# The wasi-sdk build system wants a clang and an ar binary in
# build/install/$PREFIX/bin
ln -s $MOZ_FETCHES_DIR/clang/bin build/install/wasi/bin
ln -s llvm-ar build/install/wasi/bin/ar

# Build compiler-rt
make \
  LLVM_PROJ_DIR=$LLVM_PROJ_DIR \
  PREFIX=$(grep -q BUILD_PREFIX Makefile || echo $PWD/build/install)/wasi \
  build/compiler-rt.BUILT \
  -j$(nproc)

mkdir -p $dir/lib
mv build/install/wasi/lib/clang/*/lib/wasi $dir/lib
tar --zstd -cf $artifact $dir
mkdir -p $UPLOAD_DIR
mv $artifact $UPLOAD_DIR/
