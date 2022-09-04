#!/bin/bash
set -x -e -v

PROJECT=cargo-vet

# Needed by osx-cross-linker.
export TARGET=$1

case "$TARGET" in
x86_64-unknown-linux-gnu)
    # Native Linux Build
    EXE=
    FEATURES="--features reqwest/native-tls-vendored"
    export RUSTFLAGS="-Clinker=$MOZ_FETCHES_DIR/clang/bin/clang++ -C link-arg=--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu -C link-arg=-fuse-ld=lld"
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang
    export CFLAGS="--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    ;;
*-apple-darwin)
    # Cross-compiling for Mac on Linux.
    EXE=
    FEATURES=
    export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"
    export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
    export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
    if test "$TARGET" = "aarch64-apple-darwin"; then
        export MACOSX_DEPLOYMENT_TARGET=11.0
    else
        export MACOSX_DEPLOYMENT_TARGET=10.12
    fi
    export CC="$MOZ_FETCHES_DIR/clang/bin/clang"
    export TARGET_CC="$CC -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk"
    ;;
x86_64-pc-windows-msvc)
    # Cross-compiling for Windows on Linux.
    EXE=.exe
    FEATURES=
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang-cl
    export TARGET_AR=$MOZ_FETCHES_DIR/clang/bin/llvm-lib

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    export CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER=$MOZ_FETCHES_DIR/clang/bin/lld-link
    export LD_PRELOAD=$MOZ_FETCHES_DIR/liblowercase/liblowercase.so
    export LOWERCASE_DIRS=$MOZ_FETCHES_DIR/vs
    ;;
esac

PATH="$MOZ_FETCHES_DIR/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/$PROJECT

cargo build --verbose --release --target "$TARGET" --bin $PROJECT $FEATURES

mkdir ${PROJECT}
cp target/$TARGET/release/${PROJECT}${EXE} ${PROJECT}/${PROJECT}${EXE}
tar -acf ${PROJECT}.tar.zst ${PROJECT}
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.zst $UPLOAD_DIR
