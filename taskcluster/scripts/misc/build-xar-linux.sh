#!/bin/bash
set -x -e -v

# This script is for building xar for Linux.
mkdir -p $UPLOAD_DIR

export PATH=$PATH:$MOZ_FETCHES_DIR/clang/bin
cd $MOZ_FETCHES_DIR/xar/xar

./autogen.sh --prefix=/builds/worker --enable-static
make_flags="-j$(nproc)"
make $make_flags

cd $(mktemp -d)
mkdir xar

cp $MOZ_FETCHES_DIR/xar/xar/src/xar ./xar/xar
tar caf $UPLOAD_DIR/xar.tar.zst ./xar
