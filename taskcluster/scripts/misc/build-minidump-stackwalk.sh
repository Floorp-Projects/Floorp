#!/bin/bash
set -x -e -v

# minidump-stackwalk is just one sub-crate of the rust-minidump project,
# (but it depends on other subcrates, so we *do* need the whole rust-minidump).
# We need to specifically tell Cargo to build just this binary. The rest
# of this is just making sure all the compiler toolchains are setup and
# little quirks of cross-compiling a self-contained binary.
FETCH=rust-minidump
PROJECT=minidump-stackwalk
COMPRESS_EXT=zst

# Needed by osx-cross-linker.
export TARGET=$1

case "$TARGET" in
x86_64-unknown-linux-gnu)
    # Native Linux Build
    EXE=
    # {CC,CXX} and TARGET_{CC,CXX} must be set because a build.rs file builds
    # some C and C++ code.
    export RUSTFLAGS="-Clinker=clang++ -C link-arg=--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    export CC=clang
    export CXX=clang++
    export CFLAGS="--sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    export CXXFLAGS="-D_GLIBCXX_USE_CXX11_ABI=0 --sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
    export PATH="$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/binutils/bin:$PATH"
    ;;
*-apple-darwin)
    # Cross-compiling for Mac on Linux.
    EXE=
    export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"
    export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
    export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
    if test "$TARGET" = "aarch64-apple-darwin"; then
        export MACOSX_DEPLOYMENT_TARGET=11.0
    else
        export MACOSX_DEPLOYMENT_TARGET=10.12
    fi
    export CC="$MOZ_FETCHES_DIR/clang/bin/clang"
    export TARGET_CC="$MOZ_FETCHES_DIR/clang/bin/clang -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk"
    export TARGET_CXX="$MOZ_FETCHES_DIR/clang/bin/clang++ -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk -stdlib=libc++"
    ;;
*-pc-windows-msvc)
    # Cross-compiling for Windows on Linux.
    EXE=.exe
    # Some magic that papers over differences in case-sensitivity/insensitivity on Linux
    # and Windows file systems.
    export LD_PRELOAD="/builds/worker/fetches/liblowercase/liblowercase.so"
    export LOWERCASE_DIRS="/builds/worker/fetches/vs2017_15.9.6"
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

cd $MOZ_FETCHES_DIR/$FETCH

cargo build --verbose --release --target "$TARGET" --bin $PROJECT

cd ..
mkdir $PROJECT
cp $FETCH/target/$TARGET/release/${PROJECT}${EXE} ${PROJECT}/${PROJECT}${EXE}
tar -acf ${PROJECT}.tar.$COMPRESS_EXT $PROJECT
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.$COMPRESS_EXT $UPLOAD_DIR

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
