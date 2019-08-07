#!/bin/bash
set -x -e -v

# This script is for building clang for Mac OS X targets on a Linux host,
# including native Mac OS X Compiler-RT libraries and llvm-symbolizer.

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

# these variables are used in build-clang.py
export CROSS_CCTOOLS_PATH=$MOZ_FETCHES_DIR/cctools
export CROSS_SYSROOT=$MOZ_FETCHES_DIR/MacOSX10.11.sdk
export PATH=$PATH:$CROSS_CCTOOLS_PATH/bin

# gets a bit too verbose here
set +x

cd $MOZ_FETCHES_DIR/llvm-project
python3 $GECKO_PATH/build/build-clang/build-clang.py -c $GECKO_PATH/$1 --skip-tar

# We now have a native macosx64 toolchain.
# What we want is a native linux64 toolchain which can target macosx64 and use the sanitizer dylibs.
# Overlay the linux64 toolchain that we used for this build (except llvm-symbolizer).
(
cd build/stage1
# Need the macosx64 native llvm-symbolizer since this gets shipped with sanitizer builds
mv clang/bin/llvm-symbolizer $MOZ_FETCHES_DIR/clang/bin/
cp --remove-destination -lr $MOZ_FETCHES_DIR/clang/* clang/
tar -c -J -f $MOZ_FETCHES_DIR/llvm-project/clang.tar.xz clang
)

set -x

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.* $UPLOAD_DIR
