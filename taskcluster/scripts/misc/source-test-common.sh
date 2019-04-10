#! /bin/bash -vex

set -x -e

export MOZBUILD_STATE_PATH=$HOME/workspace

# Setup toolchains
cd $MOZBUILD_STATE_PATH
$HOME/checkouts/gecko/mach artifact toolchain -v $MOZ_TOOLCHAINS

# Add toolchain binaries to PATH to run ./mach configure
export PATH=$MOZBUILD_STATE_PATH/clang/bin:$PATH
export PATH=$MOZBUILD_STATE_PATH/rustc/bin:$PATH
export PATH=$MOZBUILD_STATE_PATH/cbindgen:$PATH
export PATH=$MOZBUILD_STATE_PATH/nasm:$PATH
export PATH=$MOZBUILD_STATE_PATH/node/bin:$PATH

# Use clang as host compiler
export CC=$MOZBUILD_STATE_PATH/clang/bin/clang
export CXX=$MOZBUILD_STATE_PATH/clang/bin/clang++
