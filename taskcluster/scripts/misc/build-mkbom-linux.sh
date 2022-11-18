#!/bin/bash
set -x -e -v

# This script is for building mkbom for Linux.
mkdir -p $UPLOAD_DIR

export PATH=$PATH:$MOZ_FETCHES_DIR/clang/bin
cd $MOZ_FETCHES_DIR/bomutils

make_flags="-j$(nproc)"
make "$make_flags"

cd $(mktemp -d)
mkdir mkbom

cp $MOZ_FETCHES_DIR/bomutils/build/bin/mkbom ./mkbom/mkbom
tar caf $UPLOAD_DIR/mkbom.tar.zst ./mkbom
