#! /bin/bash -vex

set -x -e

export MOZBUILD_STATE_PATH=$HOME/workspace

# Add toolchain binaries to PATH to run ./mach configure
export PATH=$MOZ_FETCHES_DIR/clang/bin:$PATH
export PATH=$MOZ_FETCHES_DIR/rustc/bin:$PATH
export PATH=$MOZ_FETCHES_DIR/cbindgen:$PATH
export PATH=$MOZ_FETCHES_DIR/nasm:$PATH
export PATH=$MOZ_FETCHES_DIR/node/bin:$PATH

# Use clang as host compiler
export CC=$MOZ_FETCHES_DIR/clang/bin/clang
export CXX=$MOZ_FETCHES_DIR/clang/bin/clang++
