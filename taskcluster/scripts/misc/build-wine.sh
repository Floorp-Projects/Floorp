#!/bin/bash
set -x -e -v

WORKSPACE=$HOME/workspace
INSTALL_DIR=$WORKSPACE/wine

mkdir -p $INSTALL_DIR

cd $MOZ_FETCHES_DIR

# --------------
cd wine-3.0.3
./configure --prefix=$INSTALL_DIR/
make -j$(nproc)
make install

# --------------

cd $WORKSPACE/
tar caf wine.tar.xz wine

mkdir -p $UPLOAD_DIR
cp wine.tar.* $UPLOAD_DIR
