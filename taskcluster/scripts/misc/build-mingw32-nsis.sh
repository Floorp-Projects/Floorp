#!/bin/bash
set -x -e -v

# We set the INSTALL_DIR to match the directory that it will run in exactly,
# otherwise we get an NSIS error of the form:
#   checking for NSIS version...
#   DEBUG: Executing: `/home/worker/workspace/build/src/mingw32/
#   DEBUG: The command returned non-zero exit status 1.
#   DEBUG: Its error output was:
#   DEBUG: | Error: opening stub "/home/worker/workspace/mingw32/
#   DEBUG: | Error initalizing CEXEBuild: error setting
#   ERROR: Failed to get nsis version.

INSTALL_DIR=$MOZ_FETCHES_DIR/nsis

mkdir -p $INSTALL_DIR

cd $MOZ_FETCHES_DIR

export PATH="$MOZ_FETCHES_DIR/mingw32/bin:$PATH"

# --------------

cd zlib-1.2.11
make -f win32/Makefile.gcc PREFIX=i686-w64-mingw32-

cd ../nsis-3.07-src
patch -p1 < $GECKO_PATH/build/win32/nsis-no-insert-timestamp.patch
scons XGCC_W32_PREFIX=i686-w64-mingw32- ZLIB_W32=../zlib-1.2.11 SKIPUTILS="NSIS Menu" PREFIX=$INSTALL_DIR/ VERSION=3.07 install

# --------------

cd $MOZ_FETCHES_DIR

tar caf nsis.tar.zst nsis

mkdir -p $UPLOAD_DIR
cp nsis.tar.* $UPLOAD_DIR
