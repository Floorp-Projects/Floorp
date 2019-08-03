#!/bin/bash
set -e -v

# This script is for building GN.

WORKSPACE=$HOME/workspace
COMPRESS_EXT=xz

CROSS_CCTOOLS_PATH=$MOZ_FETCHES_DIR/cctools
CROSS_SYSROOT=$MOZ_FETCHES_DIR/MacOSX10.11.sdk

export CC=$MOZ_FETCHES_DIR/clang/bin/clang
export CXX=$MOZ_FETCHES_DIR/clang/bin/clang++
export AR=$MOZ_FETCHES_DIR/clang/bin/llvm-ar
export CFLAGS="-target x86_64-apple-darwin -mlinker-version=137 -B ${CROSS_CCTOOLS_PATH}/bin -isysroot ${CROSS_SYSROOT} -I${CROSS_SYSROOT}/usr/include -iframework ${CROSS_SYSROOT}/System/Library/Frameworks"
export CXXFLAGS="-stdlib=libc++ ${CFLAGS}"
export LDFLAGS="${CXXFLAGS} -Wl,-syslibroot,${CROSS_SYSROOT} -Wl,-dead_strip"

# We patch tools/gn/bootstrap/bootstrap.py to detect this.
export MAC_CROSS=1

# Gn build scripts use #!/usr/bin/env python, which will be python 2.6 on
# the worker and cause failures. Work around this by putting python2.7
# ahead of it in PATH.
mkdir -p $WORKSPACE/python_bin
ln -s /usr/bin/python2.7 $WORKSPACE/python_bin/python
export PATH=$WORKSPACE/python_bin:$PATH

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

# The ninja templates used to bootstrap gn have hard-coded references to
# 'libtool', make sure we find the right one.
ln -s $CROSS_CCTOOLS_PATH/bin/x86_64-apple-darwin-libtool $CROSS_CCTOOLS_PATH/bin/libtool
export PATH=$CROSS_CCTOOLS_PATH/bin:$PATH

. taskcluster/scripts/misc/build-gn-common.sh
