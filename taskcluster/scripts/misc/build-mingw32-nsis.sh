#!/bin/bash
set -x -e -v

INSTALL_DIR=$MOZ_FETCHES_DIR/nsis

mkdir -p $INSTALL_DIR

cd $MOZ_FETCHES_DIR

export PATH="$MOZ_FETCHES_DIR/mingw32/bin:$PATH"

# --------------

cd zlib-1.2.11
make -f win32/Makefile.gcc PREFIX=i686-w64-mingw32-

cd ../nsis-3.07-src
patch -p1 < $GECKO_PATH/build/win32/nsis-no-insert-timestamp.patch
patch -p1 < $GECKO_PATH/build/win32/nsis-no-underscore.patch
scons \
  XGCC_W32_PREFIX=i686-w64-mingw32- \
  ZLIB_W32=../zlib-1.2.11 \
  SKIPUTILS="NSIS Menu" \
  PREFIX_DEST=$INSTALL_DIR/ \
  PREFIX_BIN=bin \
  NSIS_CONFIG_CONST_DATA_PATH=no \
  VERSION=3.07 \
  install

# --------------

cd $MOZ_FETCHES_DIR

tar caf nsis.tar.zst nsis

mkdir -p $UPLOAD_DIR
cp nsis.tar.* $UPLOAD_DIR
