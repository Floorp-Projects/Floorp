#! /bin/bash -vex

set -x -e

export MOZBUILD_STATE_PATH=$HOME/workspace

# Setup toolchains
cd $MOZBUILD_STATE_PATH
$HOME/checkouts/gecko/mach artifact toolchain -v $MOZ_TOOLCHAINS

# Add toolchain binaries to PATH
export PATH=$MOZBUILD_STATE_PATH/nasm:$PATH
export PATH=$MOZBUILD_STATE_PATH/clang/bin:$PATH
export PATH=$MOZBUILD_STATE_PATH/clang-tidy/bin:$PATH
export PATH=$MOZBUILD_STATE_PATH/rustc/bin:$PATH
export PATH=$MOZBUILD_STATE_PATH/cbindgen:$PATH
export PATH=$MOZBUILD_STATE_PATH/node/bin:$PATH

# Use toolchain clang
export LD_LIBRARY_PATH=$MOZBUILD_STATE_PATH/clang/lib

# Mach lookup clang-tidy in clang-tools
mkdir -p $MOZBUILD_STATE_PATH/clang-tools
ln -s $MOZBUILD_STATE_PATH/clang-tidy $MOZBUILD_STATE_PATH/clang-tools/clang-tidy
