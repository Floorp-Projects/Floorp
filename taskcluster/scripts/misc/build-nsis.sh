#!/bin/bash
set -x -e -v

export PATH=$MOZ_FETCHES_DIR/clang/bin:$PATH

# nsis/ contains the pre-built windows native nsis. We build a linux
# makensis from source and install it there.
INSTALL_DIR=$MOZ_FETCHES_DIR/nsis

cd $MOZ_FETCHES_DIR/nsis-3.07-src
patch -p1 < $GECKO_PATH/build/win32/nsis-no-underscore.patch
scons \
  -j $(nproc) \
  PATH=$PATH \
  CC="clang --sysroot $MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu" \
  CXX="clang++ --sysroot $MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu" \
  SKIPSTUBS=all \
  SKIPPLUGINS=all \
  SKIPUTILS=all \
  SKIPMISC=all \
  PREFIX_DEST=$INSTALL_DIR/ \
  PREFIX_BIN=bin \
  NSIS_CONFIG_CONST_DATA_PATH=no \
  VERSION=3.07 \
  install-compiler

cd $MOZ_FETCHES_DIR

tar caf nsis.tar.zst nsis

mkdir -p $UPLOAD_DIR
cp nsis.tar.zst $UPLOAD_DIR
