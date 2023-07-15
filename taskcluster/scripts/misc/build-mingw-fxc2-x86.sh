#!/bin/bash
set -x -e -v

WORKSPACE=$HOME/workspace
INSTALL_DIR=$WORKSPACE/fxc2

mkdir -p $INSTALL_DIR/bin

export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"

# --------------

cd $MOZ_FETCHES_DIR/fxc2
make -j$(nproc) x86

cp fxc2.exe $INSTALL_DIR/bin/

# --------------

cd $WORKSPACE
tar caf fxc2.tar.zst fxc2

mkdir -p $UPLOAD_DIR
cp fxc2.tar.* $UPLOAD_DIR
