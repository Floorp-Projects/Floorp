#!/bin/bash
set -x -e -v

PROJECT=dump_syms
# Needed by osx-cross-linker.
export TARGET=$1

# This script is for building dump_syms
case "$(uname -s)" in
Linux)
    COMPRESS_EXT=xz
    ;;
MINGW*)
    UPLOAD_DIR=$PWD/public/build
    COMPRESS_EXT=bz2

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    ;;
esac

cd $GECKO_PATH

if [ -n "$TOOLTOOL_MANIFEST" ]; then
  . taskcluster/scripts/misc/tooltool-download.sh
fi

PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/$PROJECT

case "$(uname -s)" in
Linux)
    if [ "$TARGET" == "x86_64-apple-darwin" ]; then
        export PATH="$MOZ_FETCHES_DIR/llvm-dsymutil/bin:$PATH"
        export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
        export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
        export CC="$MOZ_FETCHES_DIR/clang/bin/clang"
        export TARGET_CC="$MOZ_FETCHES_DIR/clang/bin/clang -isysroot $MOZ_FETCHES_DIR/MacOSX10.11.sdk"
        export TARGET_CXX="$MOZ_FETCHES_DIR/clang/bin/clang++ -isysroot $MOZ_FETCHES_DIR/MacOSX10.11.sdk -stdlib=libc++"
        cargo build --verbose --release --target $TARGET
    else
        export RUSTFLAGS=-Clinker=clang++
        export CC=clang
        export CXX=clang++
        export PATH="$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/binutils/bin:$PATH"
        cargo build --verbose --release --features "vendored-openssl"
    fi
    ;;
MINGW*)
    cargo build --verbose --release
    ;;
esac

mkdir $PROJECT
PROJECT_OUT=target/release/${PROJECT}*
if [ -n "$TARGET" ]; then
    PROJECT_OUT=target/${TARGET}/release/${PROJECT}*
fi
cp ${PROJECT_OUT} ${PROJECT}/
tar -acf ${PROJECT}.tar.$COMPRESS_EXT $PROJECT
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.$COMPRESS_EXT $UPLOAD_DIR

cd ..
. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
