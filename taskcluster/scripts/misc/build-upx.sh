#!/bin/bash
set -x -e -v

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
INSTALL_DIR=$WORKSPACE/upx
UPLOAD_DIR=$HOME/artifacts

mkdir -p $INSTALL_DIR/bin

cd $WORKSPACE

# --------------

wget --progress=dot:mega http://www.oberhumer.com/opensource/ucl/download/ucl-1.03.tar.gz
echo "5847003d136fbbca1334dd5de10554c76c755f7c  ucl-1.03.tar.gz" | sha1sum -c -
tar xf ucl-1.03.tar.gz
cd ucl-1.03
./configure
make -j$(nproc)

# --------------

cd ..
git clone -n https://github.com/upx/upx.git upx-clone
cd upx-clone
git checkout d31947e1f016e87f24f88b944439bbee892f0429 # Asserts integrity of the clone (right?)
git submodule update --init --recursive
export UPX_UCLDIR=$WORKSPACE/ucl-1.03
cd src
make -j$(nproc)
cp upx.out $INSTALL_DIR/bin/upx

# --------------

cd $WORKSPACE
tar caf upx.tar.xz upx

mkdir -p $UPLOAD_DIR
cp upx.tar.* $UPLOAD_DIR
