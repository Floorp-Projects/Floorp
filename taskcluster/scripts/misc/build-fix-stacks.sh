#!/bin/bash
set -x -e -v

# This script is for building fix-stacks.
PROJECT=fix-stacks

# Needed by osx-cross-linker; unset when building for non-osx.
export TARGET="$1"

case "$(uname -s)" in
Linux)
    EXE=
    COMPRESS_EXT=xz
    # {CC,CXX} and TARGET_{CC,CXX} must be set because a build.rs file builds
    # some C and C++ code.
    export CC=$MOZ_FETCHES_DIR/clang/bin/clang
    export CXX=$MOZ_FETCHES_DIR/clang/bin/clang++
    export PATH="$MOZ_FETCHES_DIR/binutils/bin:$PATH"
    if [ "$TARGET" == "x86_64-apple-darwin" ] ; then
        # Cross-compiling for Mac on Linux.
        export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
        export RUSTFLAGS="-C linker=$GECKO_PATH/taskcluster/scripts/misc/osx-cross-linker"
        export TARGET_CC="$CC -isysroot $MOZ_FETCHES_DIR/MacOSX10.11.sdk"
        export TARGET_CXX="$CXX -isysroot $MOZ_FETCHES_DIR/MacOSX10.11.sdk"
    else
        export RUSTFLAGS="-C linker=$CXX"
    fi
    ;;
MINGW*)
    UPLOAD_DIR=$PWD/public/build
    EXE=.exe
    COMPRESS_EXT=bz2

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    ;;
esac

cd $GECKO_PATH

if [ -n "$TOOLTOOL_MANIFEST" ]; then
  . taskcluster/scripts/misc/tooltool-download.sh
fi

# cargo gets mad if the parent directory has a Cargo.toml file in it
if [ -e Cargo.toml ]; then
  mv Cargo.toml Cargo.toml.back
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
if [ -e Cargo.toml.back ]; then
  mv Cargo.toml.back Cargo.toml
fi

. $GECKO_PATH/taskcluster/scripts/misc/vs-cleanup.sh
