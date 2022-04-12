#!/bin/bash
set -x -e -v

PROJECT=rust-size
COMPRESS_EXT=zst

# This script is for building rust-size
case "$(uname -s)" in
Linux)
    ;;
MINGW*|MSYS*)
    UPLOAD_DIR=$PWD/public/build

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    ;;
esac

cd $GECKO_PATH

PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/$PROJECT

cargo build --verbose --release

mkdir $PROJECT
cp target/release/${PROJECT}* ${PROJECT}/
tar -c $PROJECT | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > ${PROJECT}.tar.$COMPRESS_EXT
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.$COMPRESS_EXT $UPLOAD_DIR

cd ..

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
