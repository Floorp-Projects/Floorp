#!/bin/bash
set -x -e -v

# This script is to repack a linux clang with Windows clang-cl.exe and compiler runtime.

cd $MOZ_FETCHES_DIR

# We already have the Linux clang extracted in $MOZ_FETCHES_DIR/clang by fetch-content
# We have a non-extracted clang.tar.bz2 for Windows clang-cl that we need to extract
# files from.

tar -jxf clang.tar.bz2 --wildcards clang/lib/clang/*/lib/windows clang/bin/clang-cl.exe
chmod +x clang/bin/clang-cl.exe
tar -Jcf clang.tar.xz clang

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.xz $UPLOAD_DIR
