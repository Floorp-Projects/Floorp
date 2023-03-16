#!/bin/sh

set -e -x

artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}
target=${dir#libunwind-}

# Make the android compiler-rt available to clang.
env UPLOAD_DIR= $GECKO_PATH/taskcluster/scripts/misc/repack-clang.sh

case "$target" in
armv7-linux-android)
  unwind_arch=arm
  ;;
aarch64-linux-android)
  unwind_arch=aarch64
  ;;
i686-linux-android)
  unwind_arch=i386
  ;;
x86_64-linux-android)
  unwind_arch=x86_64
  ;;
*)
  echo $target is not supported. >&2
  exit 1
  ;;
esac

EXTRA_CMAKE_FLAGS="
  $EXTRA_CMAKE_FLAGS
  -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF
  -DCMAKE_C_COMPILER_WORKS=1
  -DCMAKE_CXX_COMPILER_WORKS=1
  -DLLVM_ENABLE_RUNTIMES=libunwind
  -DLIBUNWIND_LIBDIR_SUFFIX=/linux/$unwind_arch
  -DLIBUNWIND_ENABLE_SHARED=OFF
"

export EXTRA_CMAKE_FLAGS

$(dirname $0)/build-llvm-common.sh runtimes install $target "$@"
