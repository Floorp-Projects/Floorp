#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
set -x -e -v

# Needed by osx-cross-linker.
export TARGET="$1"

cd $GECKO_PATH

EXE=
COMPRESS_EXT=gz

case "$TARGET" in
*windows-msvc)
  EXE=.exe
  COMPRESS_EXT=zip
  . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
  # Bug 1584530: don't require the Microsoft MSVC runtime to be installed.
  export RUSTFLAGS="-Ctarget-feature=+crt-static -C linker=$MOZ_FETCHES_DIR/clang/bin/lld-link"
  export LD_PRELOAD=$MOZ_FETCHES_DIR/liblowercase/liblowercase.so
  export LOWERCASE_DIRS=$MOZ_FETCHES_DIR/vs
  ;;
# OSX cross builds are a bit harder
*-apple-darwin)
  export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"
  export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
  export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
  if test "$TARGET" = "aarch64-apple-darwin"; then
      export MACOSX_DEPLOYMENT_TARGET=11.0
  else
      export MACOSX_DEPLOYMENT_TARGET=10.12
  fi
  ;;
aarch64-unknown-linux-musl)
  export RUSTFLAGS="-C linker=$MOZ_FETCHES_DIR/clang/bin/clang -C link-arg=--target=$TARGET -C link-arg=-fuse-ld=lld"
  ;;
esac

export PATH="$MOZ_FETCHES_DIR/rustc/bin:$PATH"

cd $GECKO_PATH/testing/geckodriver

cp $GECKO_PATH/.cargo/config.in $GECKO_PATH/.cargo/config

cargo build --frozen --verbose --release --target "$TARGET"

cd $GECKO_PATH
mkdir -p $UPLOAD_DIR

cp target/$TARGET/release/geckodriver$EXE .
if [ "$COMPRESS_EXT" = "zip" ]; then
    zip geckodriver.zip geckodriver$EXE
    cp geckodriver.zip $UPLOAD_DIR
else
    tar -acf geckodriver.tar.$COMPRESS_EXT geckodriver$EXE
    cp geckodriver.tar.$COMPRESS_EXT $UPLOAD_DIR
fi

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
