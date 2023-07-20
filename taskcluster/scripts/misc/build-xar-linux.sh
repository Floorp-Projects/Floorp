#!/bin/bash
set -x -e -v

# This script is for building xar for Linux.
mkdir -p $UPLOAD_DIR

export PATH=$PATH:$MOZ_FETCHES_DIR/clang/bin
cd $MOZ_FETCHES_DIR/xar/xar

./autogen.sh --prefix=/builds/worker --enable-static

# Force statically-linking to libcrypto. pkg-config --static will tell
# us the extra flags that are needed (in practice, -ldl -pthread),
# and -lcrypto, which we need to change to actually link statically.
CRYPTO=$(pkg-config --static --libs libcrypto | sed 's/-lcrypto/-l:libcrypto.a/')
sed -i "s/-lcrypto/$CRYPTO/" src/Makefile.inc

make_flags="-j$(nproc)"
make $make_flags

cd $(mktemp -d)
mkdir xar

cp $MOZ_FETCHES_DIR/xar/xar/src/xar ./xar/xar
tar caf $UPLOAD_DIR/xar.tar.zst ./xar
