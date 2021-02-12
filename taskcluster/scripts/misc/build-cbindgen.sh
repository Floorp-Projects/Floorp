#!/bin/bash
set -x -e -v

# Needed by osx-cross-linker.
export TARGET="$1"

case "$(uname -s)" in
Linux)
    COMPRESS_EXT=xz
    ;;
MINGW*)
    UPLOAD_DIR=$PWD/public/build
    COMPRESS_EXT=bz2

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh

    export CC=clang-cl
    ;;
esac

cd $GECKO_PATH

if [ -n "$TOOLTOOL_MANIFEST" ]; then
  . taskcluster/scripts/misc/tooltool-download.sh
fi

# OSX cross builds are a bit harder
case "$TARGET" in
*-apple-darwin)
  export PATH="$MOZ_FETCHES_DIR/llvm-dsymutil/bin:$PATH"
  export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
  export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
  if test "$TARGET" = "aarch64-apple-darwin"; then
    export SDK_VER=11.0
  fi
  ;;
x86_64-unknown-linux-gnu)
  export CC="$MOZ_FETCHES_DIR/clang/bin/clang"
  export CFLAGS_x86_64_unknown_linux_gnu="--sysroot=$MOZ_FETCHES_DIR/sysroot"
  export RUSTFLAGS="-C linker=$CC -C link-arg=--sysroot=$MOZ_FETCHES_DIR/sysroot"
  ;;
esac

export PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/cbindgen

cargo build --verbose --release --target "$TARGET"

mkdir cbindgen
cp target/$TARGET/release/cbindgen* cbindgen/
tar -acf cbindgen.tar.$COMPRESS_EXT cbindgen
mkdir -p $UPLOAD_DIR
cp cbindgen.tar.$COMPRESS_EXT $UPLOAD_DIR

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
