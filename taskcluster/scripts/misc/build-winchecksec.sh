#!/bin/bash
set -e -v -x

mkdir -p $UPLOAD_DIR

cd $MOZ_FETCHES_DIR/winchecksec

cmake \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=Off

ninja -v

cd ..
tar -caf winchecksec.tar.bz2 winchecksec/winchecksec
cp winchecksec.tar.bz2 $UPLOAD_DIR/
