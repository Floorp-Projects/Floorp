#!/bin/bash
set -x -e -v

# This script is for building fix-stacks.
PROJECT=fix-stacks

export TARGET="$1"

case "$TARGET" in
x86_64-unknown-linux-gnu)
    EXE=
    COMPRESS_EXT=xz
    # {CC,CXX} and TARGET_{CC,CXX} must be set because a build.rs file builds
    # some C and C++ code.
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang
    export CXX=$MOZ_FETCHES_DIR/clang/bin/clang++
    export PATH="$MOZ_FETCHES_DIR/binutils/bin:$PATH"
    export RUSTFLAGS="-C linker=$CXX"
    ;;
x86_64-apple-darwin)
    # Cross-compiling for Mac on Linux.
    EXE=
    COMPRESS_EXT=xz
    # {CC,CXX} and TARGET_{CC,CXX} must be set because a build.rs file builds
    # some C and C++ code.
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang
    export CXX=$MOZ_FETCHES_DIR/clang/bin/clang++
    export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
    export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
    export TARGET_CC="$CC -isysroot $MOZ_FETCHES_DIR/MacOSX10.11.sdk"
    export TARGET_CXX="$CXX -isysroot $MOZ_FETCHES_DIR/MacOSX10.11.sdk"
    ;;
i686-pc-windows-msvc)
    # Cross-compiling for Windows on Linux.
    EXE=.exe
    COMPRESS_EXT=bz2
    # Some magic that papers over differences in case-sensitivity/insensitivity on Linux
    # and Windows file systems.
    export LD_PRELOAD="/builds/worker/fetches/liblowercase/liblowercase.so"
    export LOWERCASE_DIRS="/builds/worker/fetches/vs2017_15.8.4"
    # {CC,CXX} and TARGET_{CC,CXX} must be set because a build.rs file builds
    # some C and C++ code.
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang-cl
    export CXX=$MOZ_FETCHES_DIR/clang/bin/clang-cl
    export TARGET_AR=$MOZ_FETCHES_DIR/clang/bin/llvm-lib
    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup32.sh
    export CARGO_TARGET_I686_PC_WINDOWS_MSVC_LINKER=$MOZ_FETCHES_DIR/clang/bin/lld-link
    ;;
esac

cd $GECKO_PATH

if [ -n "$TOOLTOOL_MANIFEST" ]; then
  . taskcluster/scripts/misc/tooltool-download.sh
fi

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
