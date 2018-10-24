#!/bin/bash
set -x -e -v

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
INSTALL_DIR=$WORKSPACE/fxc2
TOOLTOOL_DIR=$WORKSPACE/build/src
UPLOAD_DIR=$HOME/artifacts

mkdir -p $INSTALL_DIR/bin

cd $TOOLTOOL_DIR
. taskcluster/scripts/misc/tooltool-download.sh
export PATH="$TOOLTOOL_DIR/clang/bin:$PATH"

cd $WORKSPACE

# --------------

git clone -n https://github.com/tomrittervg/fxc2.git fxc2-clone
cd fxc2-clone
git checkout 502ef40807a472ba845f1cbdeac95ecab1aea2fd # Asserts integrity of the clone (right?)
make -j$(nproc) x86

cp fxc2.exe $INSTALL_DIR/bin/
cp dll/d3dcompiler_47_32.dll $INSTALL_DIR/bin/d3dcompiler_47.dll

# --------------

cd $WORKSPACE
tar caf fxc2.tar.xz fxc2

mkdir -p $UPLOAD_DIR
cp fxc2.tar.* $UPLOAD_DIR
