#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
set -x -e -v

# Needed by osx-cross-linker.
export TARGET="$1"

cd $GECKO_PATH

if [ -n "$TOOLTOOL_MANIFEST" ]; then
  . taskcluster/scripts/misc/tooltool-download.sh
fi

EXE=
COMPRESS_EXT=xz

case "$TARGET" in
*windows-msvc)
  EXE=.exe
  COMPRESS_EXT=bz2
  if [[ $TARGET == "i686-pc-windows-msvc" ]]; then
    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup32.sh
    export CARGO_TARGET_I686_PC_WINDOWS_MSVC_LINKER=$MOZ_FETCHES_DIR/clang/bin/lld-link
  else
    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    export CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER=$MOZ_FETCHES_DIR/clang/bin/lld-link
  fi
  ( IFS=\;
    for d in $LIB; do
      (cd "$d"; rename y/A-Z/a-z/ *)
    done
  )
  ;;
# OSX cross builds are a bit harder
x86_64-apple-darwin)
  export PATH="$MOZ_FETCHES_DIR/llvm-dsymutil/bin:$PATH"
  export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
  export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
  ;;
esac

export PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

cd $GECKO_PATH/testing/geckodriver

cargo build --frozen --verbose --release --target "$TARGET"

cd $GECKO_PATH

cp target/$TARGET/release/geckodriver$EXE .
tar -acf geckodriver.tar.$COMPRESS_EXT geckodriver$EXE
mkdir -p $UPLOAD_DIR
cp geckodriver.tar.$COMPRESS_EXT $UPLOAD_DIR

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
