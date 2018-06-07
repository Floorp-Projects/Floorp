#!/bin/bash
set -x -e -v

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
INSTALL_DIR=$WORKSPACE/wine
UPLOAD_DIR=$HOME/artifacts

mkdir -p $INSTALL_DIR

root_dir=$HOME_DIR
data_dir=$HOME_DIR/src/build/unix/build-gcc

. $data_dir/download-tools.sh

cd $WORKSPACE

# --------------
$GPG --import $data_dir/DA23579A74D4AD9AF9D3F945CEFAC8EAAF17519D.key

download_and_check http://dl.winehq.org/wine/source/2.0/ wine-2.0.1.tar.xz.sign
tar xaf $TMPDIR/wine-2.0.1.tar.xz
cd wine-2.0.1
./configure --prefix=$INSTALL_DIR/
make -j$(nproc)
make install

# --------------

cd $WORKSPACE/
tar caf wine.tar.xz wine

mkdir -p $UPLOAD_DIR
cp wine.tar.* $UPLOAD_DIR
