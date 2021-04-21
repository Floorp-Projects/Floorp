#!/bin/bash
set -x -e -v

# This script is to repack a linux clang with Windows clang-cl.exe and compiler runtime.

cd $MOZ_FETCHES_DIR

# We already have the Linux clang extracted in $MOZ_FETCHES_DIR/clang by fetch-content
# We have a non-extracted clang-cl/clang.tar.zst for Windows clang-cl that we need to extract
# files from.

$GECKO_PATH/taskcluster/scripts/misc/zstdpy -d clang-cl/clang.tar.zst | tar -x --wildcards clang/lib/clang/*/lib/windows clang/bin/clang-cl.exe clang/bin/llvm-symbolizer.exe
chmod +x clang/bin/clang-cl.exe
tar caf clang.tar.zst clang

# Put a tarball in the artifacts dir
mkdir -p $UPLOAD_DIR
cp clang.tar.zst $UPLOAD_DIR
