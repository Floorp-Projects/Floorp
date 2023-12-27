#!/bin/bash
set -e -v

# This script is for building GN.

WORKSPACE=$HOME/workspace

CROSS_SYSROOT=$MOZ_FETCHES_DIR/MacOSX14.2.sdk
export MACOSX_DEPLOYMENT_TARGET=10.12

export CC=$MOZ_FETCHES_DIR/clang/bin/clang
export CXX=$MOZ_FETCHES_DIR/clang/bin/clang++
export AR=$MOZ_FETCHES_DIR/clang/bin/llvm-ar
export CFLAGS="-target x86_64-apple-darwin -isysroot ${CROSS_SYSROOT} -I${CROSS_SYSROOT}/usr/include -iframework ${CROSS_SYSROOT}/System/Library/Frameworks"
export CXXFLAGS="-stdlib=libc++ ${CFLAGS}"
export LDFLAGS="-fuse-ld=lld ${CXXFLAGS} -Wl,-syslibroot,${CROSS_SYSROOT} -Wl,-dead_strip"

# We patch tools/gn/bootstrap/bootstrap.py to detect this.
export MAC_CROSS=1

cd $GECKO_PATH

. taskcluster/scripts/misc/build-gn-common.sh
