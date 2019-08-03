#!/bin/bash
set -x -e -v

# This script is for building grcov

PROJECT=grcov

# This script is for building rust-size
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

. taskcluster/scripts/misc/tooltool-download.sh

# cargo gets mad if the parent directory has a Cargo.toml file in it
if [ -e Cargo.toml ]; then
  mv Cargo.toml Cargo.toml.back
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
if [ -e Cargo.toml.back ]; then
  mv Cargo.toml.back Cargo.toml
fi
