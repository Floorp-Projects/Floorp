#!/bin/bash
set -x -e -v

# This script is for building fix-stacks.
PROJECT=fix-stacks

export TARGET="$1"

COMPRESS_EXT=zst
case "$TARGET" in
x86_64-unknown-linux-gnu)
    EXE=
    # {CC,CXX} and TARGET_{CC,CXX} must be set because a build.rs file builds
    # some C and C++ code.
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang
    export CXX=$MOZ_FETCHES_DIR/clang/bin/clang++
    export PATH="$MOZ_FETCHES_DIR/binutils/bin:$PATH"
    export CFLAGS_x86_64_unknown_linux_gnu="--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    export RUSTFLAGS="-C linker=$CXX -C link-arg=--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    ;;
*-apple-darwin)
    # Cross-compiling for Mac on Linux.
    EXE=
    # {CC,CXX} and TARGET_{CC,CXX} must be set because a build.rs file builds
    # some C and C++ code.
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang
    export CXX=$MOZ_FETCHES_DIR/clang/bin/clang++
    export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
    export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
    if test "$TARGET" = "aarch64-apple-darwin"; then
        export MACOSX_DEPLOYMENT_TARGET=11.0
    else
        export MACOSX_DEPLOYMENT_TARGET=10.12
    fi
    export TARGET_CC="$CC -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk"
    export TARGET_CXX="$CXX -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk"
    ;;
*-pc-windows-msvc)
    # Cross-compiling for Windows on Linux.
    EXE=.exe
    # Some magic that papers over differences in case-sensitivity/insensitivity on Linux
    # and Windows file systems.
    export LD_PRELOAD="/builds/worker/fetches/liblowercase/liblowercase.so"
    export LOWERCASE_DIRS="/builds/worker/fetches/vs"
    # {CC,CXX} and TARGET_{CC,CXX} must be set because a build.rs file builds
    # some C and C++ code.
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang-cl
    export CXX=$MOZ_FETCHES_DIR/clang/bin/clang-cl
    export TARGET_AR=$MOZ_FETCHES_DIR/clang/bin/llvm-lib

    case "$TARGET" in
        i686-pc-windows-msvc)
            export CARGO_TARGET_I686_PC_WINDOWS_MSVC_LINKER=$MOZ_FETCHES_DIR/clang/bin/lld-link
            ;;
        x86_64-pc-windows-msvc)
            export CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER=$MOZ_FETCHES_DIR/clang/bin/lld-link
            ;;
    esac
    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    ;;
esac

cd $GECKO_PATH

PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/$PROJECT

cargo build --verbose --release --target "$TARGET"

mkdir $PROJECT
cp target/$TARGET/release/${PROJECT}${EXE} ${PROJECT}/
tar -acf ${PROJECT}.tar.$COMPRESS_EXT $PROJECT
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.$COMPRESS_EXT $UPLOAD_DIR

cd ..

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
