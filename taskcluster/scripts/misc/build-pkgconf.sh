#!/bin/bash
set -x -e -v

# This script is for building pkgconfs.
PROJECT=pkgconf

cd ${MOZ_FETCHES_DIR}/${PROJECT}

case "$1" in
x86_64-unknown-linux-gnu)
    ./configure --disable-shared CC="$MOZ_FETCHES_DIR/clang/bin/clang --sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    EXE=
    ;;
x86_64-apple-darwin)
    export LD_LIBRARY_PATH="$MOZ_FETCHES_DIR/clang/lib"
    export PATH="$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/cctools/bin:$PATH"
    export MACOSX_DEPLOYMENT_TARGET=10.12
    ./configure --disable-shared CC="clang --target=x86_64-apple-darwin -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk" --host=x86_64-apple-darwin
    EXE=
    ;;
aarch64-apple-darwin)
    export LD_LIBRARY_PATH="$MOZ_FETCHES_DIR/clang/lib"
    export PATH="$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/cctools/bin:$PATH"
    export MACOSX_DEPLOYMENT_TARGET=11.0
    ./configure --disable-shared CC="clang --target=aarch64-apple-darwin -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk" --host=aarch64-apple-darwin
    EXE=
    ;;
x86_64-pc-windows-gnu)
    export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"
    ./configure --disable-shared CC=x86_64-w64-mingw32-clang AR=llvm-ar RANLIB=llvm-ranlib CPPFLAGS=-DPKGCONFIG_IS_STATIC=1 --host=x86_64-w64-mingw32
    EXE=.exe
    ;;
esac

make -j$(nproc) V=1

mv ${PROJECT}${EXE} ${PROJECT}_tmp
mkdir ${PROJECT}
mv ${PROJECT}_tmp ${PROJECT}/pkg-config${EXE}
tar -acf ${PROJECT}.tar.zst ${PROJECT}

mkdir -p $UPLOAD_DIR
mv ${PROJECT}.tar.zst $UPLOAD_DIR
