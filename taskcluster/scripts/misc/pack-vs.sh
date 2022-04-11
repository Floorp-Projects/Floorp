#!/bin/sh

set -e -x

artifact=$(basename $TOOLCHAIN_ARTIFACT)
dir=${artifact%.tar.*}

$GECKO_PATH/mach python --virtualenv build $GECKO_PATH/build/vs/pack_vs.py -o "$artifact" $GECKO_PATH/$1

mkdir -p "$UPLOAD_DIR"
mv "$artifact" "$UPLOAD_DIR"
