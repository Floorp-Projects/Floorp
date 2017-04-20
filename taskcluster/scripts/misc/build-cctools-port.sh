#!/bin/bash
set -x -e -v

# This script is for building cctools (Apple's binutils) for Linux using
# cctools-port (https://github.com/tpoechtrager/cctools-port).
WORKSPACE=$HOME/workspace
UPLOAD_DIR=$WORKSPACE/artifacts

# Repository info
: CROSSTOOL_PORT_REPOSITORY    ${CROSSTOOL_PORT_REPOSITORY:=https://github.com/tpoechtrager/cctools-port}
: CROSSTOOL_PORT_REV           ${CROSSTOOL_PORT_REV:=8e9c3f2506b51cf56725eaa60b6e90e240e249ca}

# Set some crosstools-port directories
CROSSTOOLS_SOURCE_DIR=$WORKSPACE/crosstools-port
CROSSTOOLS_CCTOOLS_DIR=$CROSSTOOLS_SOURCE_DIR/cctools
CROSSTOOLS_BUILD_DIR=$WORKSPACE/cctools
CLANG_DIR=$WORKSPACE/build/src/clang

# Create our directories
mkdir -p $CROSSTOOLS_BUILD_DIR

git clone --no-checkout $CROSSTOOL_PORT_REPOSITORY $CROSSTOOLS_SOURCE_DIR
cd $CROSSTOOLS_SOURCE_DIR
git checkout $CROSSTOOL_PORT_REV
echo "Building from commit hash `git rev-parse $CROSSTOOL_PORT_REV`..."

# Fetch clang from tooltool
cd $WORKSPACE/build/src
TOOLTOOL_MANIFEST=browser/config/tooltool-manifests/linux64/clang.manifest
. taskcluster/scripts/misc/tooltool-download.sh

# Configure crosstools-port
cd $CROSSTOOLS_CCTOOLS_DIR
export CC=$CLANG_DIR/bin/clang
export CXX=$CLANG_DIR/bin/clang++
export LDFLAGS=/lib64/libpthread.so.0
./autogen.sh
./configure --prefix=$CROSSTOOLS_BUILD_DIR --target=x86_64-apple-darwin11 --with-llvm-config=$CLANG_DIR/bin/llvm-config

# Build cctools
make -j `nproc --all` install
strip $CROSSTOOLS_BUILD_DIR/bin/*
# cctools-port doesn't include dsymutil but clang will need to find it.
cp $CLANG_DIR/bin/llvm-dsymutil $CROSSTOOLS_BUILD_DIR/bin/x86_64-apple-darwin11-dsymutil

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
tar cJf $UPLOAD_DIR/cctools.tar.xz -C $CROSSTOOLS_BUILD_DIR/.. `basename $CROSSTOOLS_BUILD_DIR`
