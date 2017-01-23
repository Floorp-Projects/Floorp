#!/bin/bash
set -x -e -v

# This script is for building cctools (Apple's binutils) for Linux using
# cctools-port (https://github.com/tpoechtrager/cctools-port).

WORKSPACE=$HOME/workspace
UPLOAD_DIR=$WORKSPACE/artifacts

# Repository info
: CROSSTOOL_PORT_REPOSITORY    ${CROSSTOOL_PORT_REPOSITORY:=https://github.com/tpoechtrager/cctools-port}
: CROSSTOOL_PORT_REV           ${CROSSTOOL_PORT_REV:=master}

# Set some crosstools-port directories
CROSSTOOLS_SOURCE_DIR=$WORKSPACE/crosstools-port
CROSSTOOLS_CCTOOLS_DIR=$CROSSTOOLS_SOURCE_DIR/cctools
CROSSTOOLS_BUILD_DIR=$WORKSPACE/cctools
CLANG_DIR=$WORKSPACE/clang

# Create our directories
mkdir -p $CROSSTOOLS_BUILD_DIR

tc-vcs checkout --force-clone $CROSSTOOLS_SOURCE_DIR $CROSSTOOL_PORT_REPOSITORY $CROSSTOOL_PORT_REPOSITORY $CROSSTOOL_PORT_REV
cd $CROSSTOOLS_SOURCE_DIR
echo "Building from commit hash `git rev-parse $CROSSTOOL_PORT_REV`..."

# Fetch clang from tooltool
cd $WORKSPACE
wget -O tooltool.py https://raw.githubusercontent.com/mozilla/build-tooltool/master/tooltool.py
chmod +x tooltool.py
: TOOLTOOL_CACHE                ${TOOLTOOL_CACHE:=/home/worker/tooltool-cache}
export TOOLTOOL_CACHE

wget ${GECKO_HEAD_REPOSITORY}/raw-file/${GECKO_HEAD_REV}/browser/config/tooltool-manifests/linux64/clang.manifest

python tooltool.py -v --manifest=clang.manifest fetch

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

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
tar cJf $UPLOAD_DIR/cctools.tar.xz -C $CROSSTOOLS_BUILD_DIR/.. `basename $CROSSTOOLS_BUILD_DIR`
