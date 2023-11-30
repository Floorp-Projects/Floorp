#!/bin/bash
set -x -e -v

WORKSPACE=$HOME/workspace
INSTALL_DIR=$WORKSPACE/upx

mkdir -p $INSTALL_DIR/bin

cd $WORKSPACE

git clone -n https://github.com/upx/upx.git upx-clone
cd upx-clone
# https://github.com/upx/upx/releases/tag/v3.95
git checkout 7a3637ff5a800b8bcbad20ae7f668d8c8449b014 # Asserts integrity of the clone (right?)
git submodule update --init --recursive
cd src
make -j$(nproc) CXXFLAGS_WERROR=
cp upx.out $INSTALL_DIR/bin/upx

# --------------

cd $WORKSPACE
tar caf upx.tar.zst upx

mkdir -p $UPLOAD_DIR
cp upx.tar.* $UPLOAD_DIR
