#!/bin/bash
set -x -e -v

# This script is for building grcov

OWNER=marco-c
PROJECT=grcov
PROJECT_REVISION=376ab00c79682a9138f54d7756e503babf140af3

WORKSPACE=$HOME/workspace
UPLOAD_DIR=$HOME/artifacts
COMPRESS_EXT=xz

PATH="$WORKSPACE/build/src/clang/bin/:$PATH"

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh

# cargo gets mad if the parent directory has a Cargo.toml file in it
if [ -e Cargo.toml ]; then
  mv Cargo.toml Cargo.toml.back
fi

PATH="$PWD/rustc/bin:$PATH"

git clone -n https://github.com/${OWNER}/${PROJECT} ${PROJECT}

pushd $PROJECT

git checkout $PROJECT_REVISION

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
