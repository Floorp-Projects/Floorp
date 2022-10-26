#!/bin/bash

# cctools sometimes needs to be rebuilt when clang is modified.
# Until bug 1471905 is addressed, increase the following number
# when a forced rebuild of cctools is necessary: 1

set -x -e -v

# This script is for building cctools (Apple's binutils) for Linux using
# cctools-port (https://github.com/tpoechtrager/cctools-port).
WORKSPACE=$HOME/workspace

# Set some crosstools-port and libtapi directories
CROSSTOOLS_SOURCE_DIR=$MOZ_FETCHES_DIR/cctools-port
CROSSTOOLS_CCTOOLS_DIR=$CROSSTOOLS_SOURCE_DIR/cctools
CROSSTOOLS_BUILD_DIR=$WORKSPACE/cctools
LIBTAPI_SOURCE_DIR=$MOZ_FETCHES_DIR/apple-libtapi
LIBTAPI_BUILD_DIR=$WORKSPACE/libtapi-build
LDID_SOURCE_DIR=$MOZ_FETCHES_DIR/ldid
CLANG_DIR=$MOZ_FETCHES_DIR/clang

# Create our directories
mkdir -p $CROSSTOOLS_BUILD_DIR $LIBTAPI_BUILD_DIR

cd $GECKO_PATH

export PATH="$MOZ_FETCHES_DIR/binutils/bin:$PATH"

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
      -DCMAKE_SYSROOT=$MOZ_FETCHES_DIR/sysroot \
      -DLLVM_TARGETS_TO_BUILD="X86;ARM;AArch64" \
      -DTAPI_REPOSITORY_STRING=$TAPI_REPOSITORY \
      -DTAPI_FULL_VERSION=$TAPI_VERSION

ninja clangBasic -v
ninja libtapi install-libtapi install-tapi-headers -v

# Setup LDFLAGS late so run-at-build-time tools in the basic clang build don't
# pick up the possibly-incompatible libstdc++ from clang.
# Also set it up such that loading libtapi doesn't require a LD_LIBRARY_PATH.
# (this requires two dollars and extra backslashing because it's used verbatim
# via a Makefile)
export LDFLAGS="-lpthread -Wl,-rpath-link,$MOZ_FETCHES_DIR/sysroot/lib/x86_64-linux-gnu -Wl,-rpath-link,$MOZ_FETCHES_DIR/sysroot/usr/lib/x86_64-linux-gnu -Wl,-rpath,\\\$\$ORIGIN/../lib,-rpath,\\\$\$ORIGIN/../../clang/lib"

export CC="$CC --sysroot=$MOZ_FETCHES_DIR/sysroot"
export CXX="$CXX --sysroot=$MOZ_FETCHES_DIR/sysroot"

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

# Build ldid
cd $LDID_SOURCE_DIR
# The crypto library in the sysroot cannot be linked in a PIE executable so we use -no-pie
make -j `nproc --all` install INSTALLPREFIX=$CROSSTOOLS_BUILD_DIR LDFLAGS="-no-pie -Wl,-Bstatic -lcrypto -Wl,-Bdynamic -ldl -pthread"

strip $CROSSTOOLS_BUILD_DIR/bin/*
# various build scripts based on cmake want to find `lipo` without a prefix
cp $CROSSTOOLS_BUILD_DIR/bin/x86_64-apple-darwin-lipo $CROSSTOOLS_BUILD_DIR/bin/lipo

(cd $CROSSTOOLS_BUILD_DIR/bin/; for i in x86_64-apple-darwin-*; do
    ln $i aarch64${i#x86_64}
done)

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
tar caf $UPLOAD_DIR/cctools.tar.zst -C $CROSSTOOLS_BUILD_DIR/.. `basename $CROSSTOOLS_BUILD_DIR`
