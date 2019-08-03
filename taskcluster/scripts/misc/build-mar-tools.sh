#!/bin/bash
set -x -e -v

# This script is for building mar and mbsdiff

COMPRESS_EXT=xz

cd $GECKO_PATH

export MOZ_OBJDIR=obj-mar

echo ac_add_options --enable-project=tools/update-packaging > .mozconfig

TOOLCHAINS="binutils clang"

for t in $TOOLCHAINS; do
    PATH="$MOZ_FETCHES_DIR/$t/bin:$PATH"
done

./mach build -v

mkdir mar-tools
cp $MOZ_OBJDIR/dist/host/bin/{mar,mbsdiff} mar-tools/

tar -acf mar-tools.tar.$COMPRESS_EXT mar-tools/
mkdir -p $UPLOAD_DIR
cp mar-tools.tar.$COMPRESS_EXT $UPLOAD_DIR
