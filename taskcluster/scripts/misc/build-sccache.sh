#!/bin/bash
set -x -e -v

# Needed by osx-cross-linker.
export TARGET="$1"

# This script is for building sccache

COMPRESS_EXT=zst
case "$(uname -s)" in
Linux)
    PATH="$MOZ_FETCHES_DIR/binutils/bin:$PATH"
    ;;
MINGW*|MSYS*)
    UPLOAD_DIR=$PWD/public/build

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    ;;
esac

cd $GECKO_PATH

PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/sccache

COMMON_FEATURES="native-zlib"

case "$(uname -s)" in
Linux)
    export CC="$MOZ_FETCHES_DIR/clang/bin/clang"
    case "$TARGET" in
    *-apple-darwin)
        export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"
        export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
        export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
        if test "$TARGET" = "aarch64-apple-darwin"; then
            export MACOSX_DEPLOYMENT_TARGET=11.0
        else
            export MACOSX_DEPLOYMENT_TARGET=10.12
        fi
        export TARGET_CC="$MOZ_FETCHES_DIR/clang/bin/clang -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk"
        cargo build --features "all $COMMON_FEATURES" --verbose --release --target $TARGET
        ;;
    *)
        export CFLAGS_x86_64_unknown_linux_gnu="--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
        export RUSTFLAGS="-C linker=$CC -C link-arg=--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
        cargo build --features "all dist-server openssl/vendored $COMMON_FEATURES" --verbose --release
        ;;
    esac

    ;;
MINGW*|MSYS*)
    cargo build --verbose --release --features="dist-client s3 gcs $COMMON_FEATURES"
    ;;
esac

SCCACHE_OUT=target/release/sccache*
if [ -n "$TARGET" ]; then
    SCCACHE_OUT=target/$TARGET/release/sccache*
fi

mkdir sccache
cp $SCCACHE_OUT sccache/
tar -c sccache | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > sccache.tar.$COMPRESS_EXT
mkdir -p $UPLOAD_DIR
cp sccache.tar.$COMPRESS_EXT $UPLOAD_DIR

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
