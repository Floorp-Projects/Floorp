#!/bin/bash
set -x -e -v

# This script is for building grcov

PROJECT=grcov
COMPRESS_EXT=zst

case "$(uname -s)" in
Linux)
    export CC=clang
    export CXX=clang++
    export RUSTFLAGS="-Clinker=clang++ -C link-arg=--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    export CXXFLAGS="-D_GLIBCXX_USE_CXX11_ABI=0 --sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    export CFLAGS="--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"

    export PATH="$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/binutils/bin:$PATH"
    ;;
MINGW*|MSYS*)
    UPLOAD_DIR=$PWD/public/build

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    ;;
esac

cd $GECKO_PATH

PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

pushd $MOZ_FETCHES_DIR/$PROJECT

cargo build --verbose --release

mkdir $PROJECT
cp target/release/${PROJECT}* ${PROJECT}/
pushd $PROJECT
tar -c * | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > ../${PROJECT}.tar.$COMPRESS_EXT
popd
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.$COMPRESS_EXT $UPLOAD_DIR

popd

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
