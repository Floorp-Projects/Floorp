#!/bin/bash
set -x -e -v

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
INSTALL_DIR=$WORKSPACE/upx
UPLOAD_DIR=$HOME/artifacts

mkdir -p $INSTALL_DIR/bin

cd $WORKSPACE

git clone -n https://github.com/upx/upx.git upx-clone
cd upx-clone
git checkout d31947e1f016e87f24f88b944439bbee892f0429 # Asserts integrity of the clone (right?)
git submodule update --init --recursive
cd src
make -j$(nproc)
cp upx.out $INSTALL_DIR/bin/upx

# --------------

cd $WORKSPACE
tar caf upx.tar.xz upx

mkdir -p $UPLOAD_DIR
cp upx.tar.* $UPLOAD_DIR
