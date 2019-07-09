#!/bin/bash
set -x -e -v

# This script is for building clang for Mac OS X targets on a Linux host,
# including native Mac OS X Compiler-RT libraries and llvm-symbolizer.
WORKSPACE=$HOME/workspace
HOME_DIR=$WORKSPACE/build
UPLOAD_DIR=$HOME/artifacts

cd $HOME_DIR/src

. taskcluster/scripts/misc/tooltool-download.sh

# ld needs libLTO.so from llvm
export LD_LIBRARY_PATH=$HOME_DIR/src/clang/lib
# these variables are used in build-clang.py
export CROSS_CCTOOLS_PATH=$HOME_DIR/src/cctools
export CROSS_SYSROOT=$HOME_DIR/src/MacOSX10.11.sdk
export PATH=$PATH:$CROSS_CCTOOLS_PATH/bin

# gets a bit too verbose here
set +x

cd build/build-clang
# |mach python| sets up a virtualenv for us!
../../mach python ./build-clang.py -c clang-8-macosx64.json --skip-tar

# We now have a native macosx64 toolchain.
# What we want is a native linux64 toolchain which can target macosx64 and use the sanitizer dylibs.
# Overlay the linux64 toolchain that we used for this build (except llvm-symbolizer).
(
cd "$WORKSPACE/moz-toolchain/build/stage1"
# Need the macosx64 native llvm-symbolizer since this gets shipped with sanitizer builds
mv clang/bin/llvm-symbolizer $HOME_DIR/src/clang/bin/
cp --remove-destination -lr $HOME_DIR/src/clang/* clang/
tar -c -J -f $HOME_DIR/src/build/build-clang/clang.tar.xz clang
)

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.* $UPLOAD_DIR
