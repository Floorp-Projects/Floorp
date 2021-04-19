#!/bin/bash
set -x -e -v

# This script is for building libbreakpadinjector.so, currently for linux only

COMPRESS_EXT=xz

cd $GECKO_PATH

export MOZ_OBJDIR=obj-injector

echo ac_add_options --enable-project=tools/crashreporter/injector > .mozconfig

INJECTOR=libbreakpadinjector.so

TOOLCHAINS="binutils rustc clang"

for t in $TOOLCHAINS; do
    PATH="$MOZ_FETCHES_DIR/$t/bin:$PATH"
done

./mach build -v

mkdir injector
cp $MOZ_OBJDIR/dist/bin/$INJECTOR injector/

tar -acf injector.tar.$COMPRESS_EXT injector/
mkdir -p $UPLOAD_DIR
cp injector.tar.$COMPRESS_EXT $UPLOAD_DIR
