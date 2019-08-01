#!/bin/bash
set -x -e -v

# This script is for building clang for Mac OS X on Linux.

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

# these variables are used in build-clang.py
export CROSS_CCTOOLS_PATH=$GECKO_PATH/cctools
export CROSS_SYSROOT=$GECKO_PATH/MacOSX10.11.sdk
export PATH=$PATH:$CROSS_CCTOOLS_PATH/bin

# gets a bit too verbose here
set +x

cd build/build-clang
python3 ./build-clang.py -c clang-8-macosx64.json

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.* $UPLOAD_DIR
