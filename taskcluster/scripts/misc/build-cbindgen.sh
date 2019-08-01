#!/bin/bash
set -x -e -v

TARGET="$1"

case "$(uname -s)" in
Linux)
    WORKSPACE=$HOME/workspace
    COMPRESS_EXT=xz
    ;;
MINGW*)
    WORKSPACE=$PWD
    UPLOAD_DIR=$WORKSPACE/public/build
    COMPRESS_EXT=bz2

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh

    export CC=clang-cl
    ;;
esac

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh

# OSX cross builds are a bit harder
if [ "$TARGET" == "x86_64-apple-darwin" ]; then
  export PATH="$PWD/llvm-dsymutil/bin:$PATH"
  export PATH="$PWD/cctools/bin:$PATH"
  cat >cross-linker <<EOF
exec $PWD/clang/bin/clang -v \
  -fuse-ld=$PWD/cctools/bin/x86_64-apple-darwin-ld \
  -mmacosx-version-min=10.11 \
  -target $TARGET \
  -B $PWD/cctools/bin \
  -isysroot $PWD/MacOSX10.11.sdk \
  "\$@"
EOF
  chmod +x cross-linker
  export RUSTFLAGS="-C linker=$PWD/cross-linker"
fi

export PATH="$PWD/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/cbindgen

cargo build --verbose --release --target "$TARGET"

mkdir cbindgen
cp target/$TARGET/release/cbindgen* cbindgen/
tar -acf cbindgen.tar.$COMPRESS_EXT cbindgen
mkdir -p $UPLOAD_DIR
cp cbindgen.tar.$COMPRESS_EXT $UPLOAD_DIR
