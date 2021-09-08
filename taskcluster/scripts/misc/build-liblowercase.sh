#!/bin/bash
set -x -e -v

: WORKSPACE ${WORKSPACE:=/builds/worker/workspace}

# This script is for building liblowercase

PATH="$MOZ_FETCHES_DIR/rustc/bin:$PATH"

cd $GECKO_PATH/build/liblowercase

cargo build --verbose --release --target-dir $WORKSPACE/obj

mkdir $WORKSPACE/liblowercase
cp $WORKSPACE/obj/release/liblowercase.so $WORKSPACE/liblowercase
strip $WORKSPACE/liblowercase/liblowercase.so
tar -C $WORKSPACE -acf $WORKSPACE/liblowercase.tar.xz liblowercase
mkdir -p $UPLOAD_DIR
cp $WORKSPACE/liblowercase.tar.xz $UPLOAD_DIR
