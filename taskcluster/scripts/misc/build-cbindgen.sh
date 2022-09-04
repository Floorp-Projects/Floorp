#!/bin/bash
set -x -e -v

# Needed by osx-cross-linker.
export TARGET="$1"
COMPRESS_EXT=zst

case "$(uname -s)" in
MINGW*|MSYS*)
    UPLOAD_DIR=$PWD/public/build

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh

    export CC=clang-cl
    ;;
esac

cd $GECKO_PATH

# OSX cross builds are a bit harder
case "$TARGET" in
*-apple-darwin)
  export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"
  export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
  export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
  ;;
x86_64-unknown-linux-gnu)
  export CC="$MOZ_FETCHES_DIR/clang/bin/clang"
  export CFLAGS_x86_64_unknown_linux_gnu="--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
  export RUSTFLAGS="-C linker=$CC -C link-arg=--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
  ;;
esac

export PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/cbindgen

cargo build --verbose --release --target "$TARGET"

mkdir cbindgen
cp target/$TARGET/release/cbindgen* cbindgen/
tar -c cbindgen | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > cbindgen.tar.$COMPRESS_EXT
mkdir -p $UPLOAD_DIR
cp cbindgen.tar.$COMPRESS_EXT $UPLOAD_DIR

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
