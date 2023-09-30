#!/bin/bash
set -x -e -v

# This script is for building pkgconfs.
PROJECT=pkgconf

cd ${MOZ_FETCHES_DIR}/${PROJECT}

export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"

case "$1" in
x86_64-unknown-linux-gnu)
    CC="clang --sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    EXE=
    ;;
x86_64-apple-darwin)
    export MACOSX_DEPLOYMENT_TARGET=10.12
    TARGET=$1
    CC="clang --target=$TARGET -isysroot $MOZ_FETCHES_DIR/MacOSX14.0.sdk"
    EXE=
    ;;
aarch64-apple-darwin)
    export MACOSX_DEPLOYMENT_TARGET=11.0
    TARGET=$1
    CC="clang --target=$TARGET -isysroot $MOZ_FETCHES_DIR/MacOSX14.0.sdk"
    EXE=
    ;;
x86_64-pc-windows-gnu)
    TARGET=x86_64-w64-mingw32
    CC="x86_64-w64-mingw32-clang -DPKGCONFIG_IS_STATIC=1"
    EXE=.exe
    ;;
esac

./configure --disable-shared CC="$CC" AR=llvm-ar RANLIB=llvm-ranlib LDFLAGS=-fuse-ld=lld ${TARGET:+--host=$TARGET}
make -j$(nproc) V=1

mv ${PROJECT}${EXE} ${PROJECT}_tmp
mkdir ${PROJECT}
mv ${PROJECT}_tmp ${PROJECT}/pkg-config${EXE}
tar -acf ${PROJECT}.tar.zst ${PROJECT}

mkdir -p $UPLOAD_DIR
mv ${PROJECT}.tar.zst $UPLOAD_DIR
