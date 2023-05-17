#!/bin/sh

set -e -x

artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}
target=${dir#compiler-rt-}

case "$target" in
*-linux-android)
  EXTRA_CMAKE_FLAGS="
    -DCOMPILER_RT_BUILD_LIBFUZZER=OFF
    -DCOMPILER_RT_BUILD_ORC=OFF
    -DCOMPILER_RT_BUILTINS_HIDE_SYMBOLS=OFF
  "
  ;;
*-apple-darwin)
  EXTRA_CMAKE_FLAGS="
    -DCOMPILER_RT_ENABLE_IOS=OFF
    -DCOMPILER_RT_ENABLE_WATCHOS=OFF
    -DCOMPILER_RT_ENABLE_TVOS=OFF
  "
  ;;
*-windows-msvc)
  EXTRA_CMAKE_FLAGS="
    -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
  "
  ;;
esac

EXTRA_CMAKE_FLAGS="
  $EXTRA_CMAKE_FLAGS
  -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON
  -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF
"

export EXTRA_CMAKE_FLAGS

$(dirname $0)/build-llvm-common.sh compiler-rt install $target "$@"
