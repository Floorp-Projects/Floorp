#!/bin/bash

# cctools sometimes needs to be rebuilt when clang is modified.
# Until bug 1471905 is addressed, increase the following number
# when a forced rebuild of cctools is necessary: 1

set -x -e -v

# This script is for building cctools (Apple's binutils) for Linux using
# cctools-port (https://github.com/tpoechtrager/cctools-port).
WORKSPACE=$HOME/workspace
UPLOAD_DIR=$HOME/artifacts

# Repository info
: CROSSTOOL_PORT_REPOSITORY    ${CROSSTOOL_PORT_REPOSITORY:=https://github.com/tpoechtrager/cctools-port}
: CROSSTOOL_PORT_REV           ${CROSSTOOL_PORT_REV:=3f979bbcd7ee29d79fb93f829edf3d1d16441147}
: LIBTAPI_REPOSITORY           ${LIBTAPI_REPOSITORY:=https://github.com/tpoechtrager/apple-libtapi}
: LIBTAPI_REV                  ${LIBTAPI_REV:=3efb201881e7a76a21e0554906cf306432539cef}

# Set some crosstools-port and libtapi directories
CROSSTOOLS_SOURCE_DIR=$WORKSPACE/crosstools-port
CROSSTOOLS_CCTOOLS_DIR=$CROSSTOOLS_SOURCE_DIR/cctools
CROSSTOOLS_BUILD_DIR=$WORKSPACE/cctools
LIBTAPI_SOURCE_DIR=$WORKSPACE/apple-libtapi
LIBTAPI_BUILD_DIR=$WORKSPACE/libtapi-build
CLANG_DIR=$WORKSPACE/build/src/clang

# Create our directories
mkdir -p $CROSSTOOLS_BUILD_DIR $LIBTAPI_BUILD_DIR

# Check for checkouts first to make interactive usage on taskcluster nicer.
if [ ! -d $CROSSTOOLS_SOURCE_DIR ]; then
    git clone --no-checkout $CROSSTOOL_PORT_REPOSITORY $CROSSTOOLS_SOURCE_DIR
fi
cd $CROSSTOOLS_SOURCE_DIR
git checkout $CROSSTOOL_PORT_REV
echo "Building cctools from commit hash `git rev-parse $CROSSTOOL_PORT_REV`..."

if [ ! -d $LIBTAPI_SOURCE_DIR ]; then
    git clone --no-checkout $LIBTAPI_REPOSITORY $LIBTAPI_SOURCE_DIR
fi
cd $LIBTAPI_SOURCE_DIR
git checkout $LIBTAPI_REV
echo "Building libtapi from commit hash `git rev-parse $LIBTAPI_REV`..."

# Fetch clang from tooltool
cd $WORKSPACE/build/src
. taskcluster/scripts/misc/tooltool-download.sh

export PATH="$WORKSPACE/build/src/binutils/bin:$PATH"

# Common setup for libtapi and cctools
export CC=$CLANG_DIR/bin/clang
export CXX=$CLANG_DIR/bin/clang++
# We also need this LD_LIBRARY_PATH at build time, since tapi builds bits of
# clang build tools, and then executes those tools.
export LD_LIBRARY_PATH=$CLANG_DIR/lib

# Build libtapi; the included build.sh is not sufficient for our purposes.
cd $LIBTAPI_BUILD_DIR

# Values taken from build.sh
TAPI_REPOSITORY=tapi-1000.10.8
TAPI_VERSION=10.0.0

INCLUDE_FIX="-I $LIBTAPI_SOURCE_DIR/src/llvm/projects/clang/include -I $PWD/projects/clang/include"

cmake $LIBTAPI_SOURCE_DIR/src/llvm \
      -GNinja \
      -DCMAKE_CXX_FLAGS="$INCLUDE_FIX" \
      -DLLVM_INCLUDE_TESTS=OFF \
      -DCMAKE_BUILD_TYPE=RELEASE \
      -DCMAKE_INSTALL_PREFIX=$CROSSTOOLS_BUILD_DIR \
      -DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64" \
      -DTAPI_REPOSITORY_STRING=$TAPI_REPOSITORY \
      -DTAPI_FULL_VERSION=$TAPI_VERSION

ninja clangBasic
ninja libtapi install-libtapi install-tapi-headers

# Setup LDFLAGS late so run-at-build-time tools in the basic clang build don't
# pick up the possibly-incompatible libstdc++ from clang.
# Also set it up such that loading libtapi doesn't require a LD_LIBRARY_PATH.
# (this requires two dollars and extra backslashing because it's used verbatim
# via a Makefile)
export LDFLAGS="-lpthread -Wl,-rpath-link,$CLANG_DIR/lib -Wl,-rpath,\\\$\$ORIGIN/../lib,-rpath,\\\$\$ORIGIN/../../clang/lib"

# Configure crosstools-port
cd $CROSSTOOLS_CCTOOLS_DIR
# Force re-libtoolization to overwrite files with the new libtool bits.
perl -pi -e 's/(LIBTOOLIZE -c)/\1 -f/' autogen.sh
./autogen.sh
./configure \
    --prefix=$CROSSTOOLS_BUILD_DIR \
    --target=x86_64-apple-darwin \
    --with-llvm-config=$CLANG_DIR/bin/llvm-config \
    --enable-lto-support \
    --enable-tapi-support \
    --with-libtapi=$CROSSTOOLS_BUILD_DIR

# Build cctools
make -j `nproc --all` install
strip $CROSSTOOLS_BUILD_DIR/bin/*
# cctools-port doesn't include dsymutil but clang will need to find it.
cp $CLANG_DIR/bin/dsymutil $CROSSTOOLS_BUILD_DIR/bin/x86_64-apple-darwin-dsymutil
# various build scripts based on cmake want to find `lipo` without a prefix
cp $CROSSTOOLS_BUILD_DIR/bin/x86_64-apple-darwin-lipo $CROSSTOOLS_BUILD_DIR/bin/lipo

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
tar cJf $UPLOAD_DIR/cctools.tar.xz -C $CROSSTOOLS_BUILD_DIR/.. `basename $CROSSTOOLS_BUILD_DIR`
