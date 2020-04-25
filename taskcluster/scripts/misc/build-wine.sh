#!/bin/bash
set -x -e -v

WORKSPACE=$HOME/workspace
INSTALL_DIR=$WORKSPACE/wine

mkdir -p $INSTALL_DIR
mkdir -p $WORKSPACE/build/wine
mkdir -p $WORKSPACE/build/wine64

cd $WORKSPACE/build/wine64
$MOZ_FETCHES_DIR/wine-5.0/configure --enable-win64 --without-x --without-freetype --prefix=$INSTALL_DIR/
make -j$(nproc)

cd $WORKSPACE/build/wine
$MOZ_FETCHES_DIR/wine-5.0/configure --with-wine64=../wine64 --without-x --without-freetype --prefix=$INSTALL_DIR/
make -j$(nproc)
make install

cd $WORKSPACE/build/wine64
make install

# --------------

cd $WORKSPACE/
tar caf wine.tar.xz wine

mkdir -p $UPLOAD_DIR
cp wine.tar.* $UPLOAD_DIR
