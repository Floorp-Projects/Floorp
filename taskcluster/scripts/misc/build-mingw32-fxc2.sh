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
export PATH="$TOOLTOOL_DIR/mingw32/bin:$PATH"

cd $WORKSPACE

# --------------

git clone -n https://github.com/mozilla/fxc2.git fxc2-clone
cd fxc2-clone
git checkout 82527b81104e5e21390d3ddcd328700c67ce73d4 # Asserts integrity of the clone (right?)
make -j$(nproc)

cp fxc2.exe $INSTALL_DIR/bin/
cp d3dcompiler_47.dll $INSTALL_DIR/bin/
cp $TOOLTOOL_DIR/mingw32/i686-w64-mingw32/bin/libwinpthread-1.dll $INSTALL_DIR/bin/

# --------------

cd $WORKSPACE
tar caf fxc2.tar.xz fxc2

mkdir -p $UPLOAD_DIR
cp fxc2.tar.* $UPLOAD_DIR
