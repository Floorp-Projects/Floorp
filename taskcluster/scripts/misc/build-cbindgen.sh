#!/bin/bash
set -x -e -v

TARGET="$1"

case "$(uname -s)" in
Linux)
    COMPRESS_EXT=xz
    ;;
MINGW*)
    UPLOAD_DIR=$PWD/public/build
    COMPRESS_EXT=bz2

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh

    export CC=clang-cl
    ;;
esac

cd $GECKO_PATH

. taskcluster/scripts/misc/tooltool-download.sh

# OSX cross builds are a bit harder
if [ "$TARGET" == "x86_64-apple-darwin" ]; then
  export PATH="$MOZ_FETCHES_DIR/llvm-dsymutil/bin:$PATH"
  export PATH="$MOZ_FETCHES_DIR/cctools/bin:$PATH"
  cat >cross-linker <<EOF
exec $MOZ_FETCHES_DIR/clang/bin/clang -v \
  -fuse-ld=$MOZ_FETCHES_DIR/cctools/bin/x86_64-apple-darwin-ld \
  -mmacosx-version-min=10.11 \
  -target $TARGET \
  -B $MOZ_FETCHES_DIR/cctools/bin \
  -isysroot $MOZ_FETCHES_DIR/MacOSX10.11.sdk \
  "\$@"
EOF
  chmod +x cross-linker
  export RUSTFLAGS="-C linker=$PWD/cross-linker"
fi

export PATH="$(cd $MOZ_FETCHES_DIR && pwd)/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/cbindgen

cargo build --verbose --release --target "$TARGET"

mkdir cbindgen
cp target/$TARGET/release/cbindgen* cbindgen/
tar -acf cbindgen.tar.$COMPRESS_EXT cbindgen
mkdir -p $UPLOAD_DIR
cp cbindgen.tar.$COMPRESS_EXT $UPLOAD_DIR
