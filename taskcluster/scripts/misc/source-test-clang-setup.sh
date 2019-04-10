#!/bin/bash
source $HOME/checkouts/gecko/taskcluster/scripts/misc/source-test-common.sh

# Add clang-tidy to PATH
export PATH=$MOZBUILD_STATE_PATH/clang-tidy/bin:$PATH

# Use toolchain clang
export LD_LIBRARY_PATH=$MOZBUILD_STATE_PATH/clang/lib

# Mach lookup clang-tidy in clang-tools
mkdir -p $MOZBUILD_STATE_PATH/clang-tools
ln -s $MOZBUILD_STATE_PATH/clang-tidy $MOZBUILD_STATE_PATH/clang-tools/clang-tidy
