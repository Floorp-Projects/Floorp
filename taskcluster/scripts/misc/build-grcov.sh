#!/bin/bash
set -x -e -v

# This script is for building grcov

PROJECT=grcov

case "$(uname -s)" in
Linux)
    COMPRESS_EXT=xz

    export CC=clang
    export CXX=clang++
    export RUSTFLAGS=-Clinker=clang++
    export CXXFLAGS=-D_GLIBCXX_USE_CXX11_ABI=0

    export PATH="$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/binutils/bin:$PATH"
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

pushd $MOZ_FETCHES_DIR/$PROJECT

cargo build --verbose --release

mkdir $PROJECT
cp target/release/${PROJECT}* ${PROJECT}/
pushd $PROJECT
tar -acf ../${PROJECT}.tar.$COMPRESS_EXT *
popd
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.$COMPRESS_EXT $UPLOAD_DIR

popd

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
