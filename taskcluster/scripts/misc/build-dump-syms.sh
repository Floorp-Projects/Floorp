#!/bin/bash
set -x -e -v

PROJECT=dump_syms
# Needed by osx-cross-linker.
export TARGET=$1

COMPRESS_EXT=zst
# This script is for building dump_syms
case "$(uname -s)" in
MINGW*|MSYS*)
    UPLOAD_DIR=$PWD/public/build

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    ;;
esac

cd $GECKO_PATH

PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/$PROJECT

case "$(uname -s)" in
Linux)
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
        export CC="$MOZ_FETCHES_DIR/clang/bin/clang"
        export TARGET_CC="$MOZ_FETCHES_DIR/clang/bin/clang -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk"
        export TARGET_CXX="$MOZ_FETCHES_DIR/clang/bin/clang++ -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk -stdlib=libc++"
        cargo build --verbose --release --target $TARGET
        ;;
    *)
        export RUSTFLAGS="-Clinker=clang++ -C link-arg=--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
        export CC=clang
        export CXX=clang++
        export CFLAGS="--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
        export CXXFLAGS="-D_GLIBCXX_USE_CXX11_ABI=0 --sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
        export PATH="$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/binutils/bin:$PATH"
        cargo build --verbose --release
        ;;
    esac
    ;;
MINGW*|MSYS*)
    cargo build --verbose --release
    ;;
esac

mkdir $PROJECT
PROJECT_OUT=target/release/${PROJECT}*
if [ -n "$TARGET" ]; then
    PROJECT_OUT=target/${TARGET}/release/${PROJECT}*
fi
cp ${PROJECT_OUT} ${PROJECT}/
tar -c $PROJECT | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > ${PROJECT}.tar.$COMPRESS_EXT
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.$COMPRESS_EXT $UPLOAD_DIR

cd ..
. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
