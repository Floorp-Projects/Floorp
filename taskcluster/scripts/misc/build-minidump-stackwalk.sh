#!/bin/bash
set -x -e -v

# This script is for building minidump_stackwalk

COMPRESS_EXT=xz

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

export MOZ_OBJDIR=obj-minidump

echo ac_add_options --enable-project=tools/crashreporter > .mozconfig

MINIDUMP_STACKWALK=minidump_stackwalk

case "$1" in
macosx64)
    TOOLCHAINS="cctools rustc clang"
    echo ac_add_options --target=x86_64-apple-darwin >> .mozconfig
    echo ac_add_options --with-macos-sdk=$MOZ_FETCHES_DIR/MacOSX10.11.sdk >> .mozconfig
    ;;
mingw32)
    TOOLCHAINS="binutils rustc clang"
    echo ac_add_options --target=i686-w64-mingw32 >> .mozconfig
    echo export CC=i686-w64-mingw32-clang >> .mozconfig
    echo export HOST_CC=clang >> .mozconfig
    MINIDUMP_STACKWALK=minidump_stackwalk.exe
    ;;
*)
    TOOLCHAINS="binutils rustc clang"
    ;;
esac

for t in $TOOLCHAINS; do
    PATH="$MOZ_FETCHES_DIR/$t/bin:$PATH"
done

./mach build -v

mkdir minidump_stackwalk
cp $MOZ_OBJDIR/dist/bin/$MINIDUMP_STACKWALK minidump_stackwalk/

tar -acf minidump_stackwalk.tar.$COMPRESS_EXT minidump_stackwalk/
mkdir -p $UPLOAD_DIR
cp minidump_stackwalk.tar.$COMPRESS_EXT $UPLOAD_DIR
