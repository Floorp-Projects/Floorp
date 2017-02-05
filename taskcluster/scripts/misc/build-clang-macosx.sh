#!/bin/bash
set -x -e -v

# This script is for building clang for Mac OS X on Linux.

WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$WORKSPACE/artifacts

cd $HOME_DIR/src

TOOLTOOL_MANIFEST=browser/config/tooltool-manifests/macosx64/cross-clang.manifest
. taskcluster/scripts/misc/tooltool-download.sh

# ld needs libLTO.so from llvm
export LD_LIBRARY_PATH=$HOME_DIR/src/clang/lib
# these variables are used in build-clang.py
export CROSS_CCTOOLS_PATH=$HOME_DIR/src/cctools
export CROSS_SYSROOT=$HOME_DIR/src/MacOSX10.10.sdk
# cmake doesn't allow us to specify a path to lipo on the command line.
ln -sf $CROSS_CCTOOLS_PATH/bin/x86_64-apple-darwin11-lipo /usr/bin/lipo

# gets a bit too verbose here
set +x

cd build/build-clang
# |mach python| sets up a virtualenv for us!
../../mach python ./build-clang.py -c clang-static-analysis-macosx64.json

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.* $UPLOAD_DIR
