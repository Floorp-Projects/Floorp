#!/bin/bash
set -x -e -v

# This script is for building clang for Mac OS X on Linux.
WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$WORKSPACE/artifacts

cd $HOME_DIR/src

. taskcluster/scripts/misc/tooltool-download.sh

# ld needs libLTO.so from llvm
export LD_LIBRARY_PATH=$HOME_DIR/src/clang/lib
# these variables are used in build-clang.py
export CROSS_CCTOOLS_PATH=$HOME_DIR/src/cctools
export CROSS_SYSROOT=$HOME_DIR/src/MacOSX10.10.sdk
# cmake doesn't allow us to specify a path to lipo on the command line.
export PATH=$PATH:$CROSS_CCTOOLS_PATH/bin
ln -sf $CROSS_CCTOOLS_PATH/bin/x86_64-apple-darwin11-lipo $CROSS_CCTOOLS_PATH/bin/lipo

# gets a bit too verbose here
set +x

cd build/build-clang
# |mach python| sets up a virtualenv for us!
../../mach python ./build-clang.py -c clang-tidy-macosx64.json

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang-tidy.tar.* $UPLOAD_DIR
