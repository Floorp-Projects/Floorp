#!/bin/bash
set -x -e -v

artifact=$(basename $TOOLCHAIN_ARTIFACT)
sysroot=${artifact%.tar.*}

cd $MOZ_FETCHES_DIR/wasi-sdk
LLVM_PROJ_DIR=$MOZ_FETCHES_DIR/llvm-project

mkdir -p build/install/wasi
# The wasi-sdk build system wants to build clang itself. We trick it into
# thinking it did, and put our own clang where it would have built its own.
ln -s $MOZ_FETCHES_DIR/clang build/llvm
touch build/llvm.BUILT

# The wasi-sdk build system has a dependency on compiler-rt for libcxxabi,
# but that's not actually necessary. Pretend it's already built
touch build/compiler-rt.BUILT

# The wasi-sdk build system wants a clang and an ar binary in
# build/install/$PREFIX/bin
ln -s $MOZ_FETCHES_DIR/clang/bin build/install/wasi/bin
ln -s llvm-ar build/install/wasi/bin/ar

# Build wasi-libc, libc++ and libc++abi.
make \
  LLVM_PROJ_DIR=$LLVM_PROJ_DIR \
  PREFIX=/wasi \
  build/wasi-libc.BUILT \
  build/libcxx.BUILT \
  build/libcxxabi.BUILT \
  -j$(nproc)

mv build/install/wasi/share/wasi-sysroot $sysroot
tar --zstd -cf $artifact $sysroot
mkdir -p $UPLOAD_DIR
mv $artifact $UPLOAD_DIR/
