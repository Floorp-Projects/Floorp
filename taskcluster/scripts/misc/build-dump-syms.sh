#!/bin/bash
set -x -e -v

PROJECT=dump_syms

# This script is for building dump_syms
case "$(uname -s)" in
Linux)
    COMPRESS_EXT=xz
    export RUSTFLAGS=-Clinker=clang++
    export CXX=clang++
    export PATH=$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/binutils/bin:$PATH
    FEATURES=vendored-openssl
    ;;
MINGW*)
    UPLOAD_DIR=$PWD/public/build
    COMPRESS_EXT=bz2
    FEATURES=

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    ;;
esac

cd $GECKO_PATH

if [ -n "$TOOLTOOL_MANIFEST" ]; then
  . taskcluster/scripts/misc/tooltool-download.sh
fi

PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/$PROJECT

cargo build --verbose --release ${FEATURES:+--features "$FEATURES"}

mkdir $PROJECT
cp target/release/${PROJECT}* ${PROJECT}/
tar -acf ${PROJECT}.tar.$COMPRESS_EXT $PROJECT
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.$COMPRESS_EXT $UPLOAD_DIR

cd ..
. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
