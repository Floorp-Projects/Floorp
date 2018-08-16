#!/bin/bash
set -e -v

# This script is for building tup on Linux.

TUP_REVISION=f77dbd429d6157dac252014a26f6d275dce85d85

WORKSPACE=$HOME/workspace
UPLOAD_DIR=$HOME/artifacts
COMPRESS_EXT=xz
export PATH=$WORKSPACE/build/src/gcc/bin:$PATH

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh

git clone https://github.com/gittup/tup.git
cd tup
git checkout $TUP_REVISION
echo 'CONFIG_TUP_SERVER=ldpreload' > tup.config
./bootstrap-ldpreload.sh
cd ..
tar caf tup.tar.xz tup/tup tup/tup-ldpreload.so tup/tup.1
cp tup.tar.xz $UPLOAD_DIR
