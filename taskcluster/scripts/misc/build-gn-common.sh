#!/bin/bash
set -e -v

# This is shared code for building GN.

# Each is a recent commit from chromium's master branch.
: CHROMIUM_REV           ${CHROMIUM_REV:=e6ba81e00ae835946e069e5bd80bd533b11d8442}
: GTEST_REV              ${GTEST_REV:=6c5116014ce51ef3273d800cbf75fcef99e798c6}
: CHROMIUM_SRC_REV       ${CHROMIUM_SRC_REV:=c338d43f49c0d72e69cd6e40eeaf4c0597dbdda1}


git clone --no-checkout https://chromium.googlesource.com/chromium/src $WORKSPACE/gn-standalone
cd $WORKSPACE/gn-standalone
git checkout $CHROMIUM_SRC_REV

git clone --no-checkout https://chromium.googlesource.com/chromium/chromium chromium_checkout
cd chromium_checkout
git checkout $CHROMIUM_REV
mkdir -p ../third_party
mv third_party/libevent ../third_party
cd ..

rm -rf testing
mkdir testing
cd testing
git clone https://chromium.googlesource.com/chromium/testing/gtest
cd gtest
git checkout $GTEST_REV
cd ../..

cd tools/gn
patch -p1 < $WORKSPACE/build/src/taskcluster/scripts/misc/gn.patch

./bootstrap/bootstrap.py -s
cd ../..

STAGE=gn
mkdir -p $UPLOAD_DIR $STAGE

# At this point, the resulting binary is at:
# $WORKSPACE/out/Release/gn
if test "$MAC_CROSS" = "" -a "$(uname)" = "Linux"; then
    strip out/Release/gn
fi
cp out/Release/gn $STAGE

tar -acf gn.tar.$COMPRESS_EXT $STAGE
cp gn.tar.$COMPRESS_EXT $UPLOAD_DIR
