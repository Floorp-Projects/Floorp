#!/bin/bash
set -x -e -v

# If you update this, make sure to update the minimum version in
# build/moz.configure/bindgen.configure as well.
CBINDGEN_REVISION=23a991a5b21e89aa1dcdc70f1371be20c93ece8e # v0.8.7
TARGET="$1"

case "$(uname -s)" in
Linux)
    WORKSPACE=$HOME/workspace
    UPLOAD_DIR=$HOME/artifacts
    COMPRESS_EXT=xz
    ;;
MINGW*)
    WORKSPACE=$PWD
    UPLOAD_DIR=$WORKSPACE/public/build
    WIN_WORKSPACE="$(pwd -W)"
    COMPRESS_EXT=bz2

    export INCLUDE="$WIN_WORKSPACE/build/src/vs2017_15.8.4/VC/include;$WIN_WORKSPACE/build/src/vs2017_15.8.4/VC/atlmfc/include;$WIN_WORKSPACE/build/src/vs2017_15.8.4/SDK/Include/10.0.17134.0/ucrt;$WIN_WORKSPACE/build/src/vs2017_15.8.4/SDK/Include/10.0.17134.0/shared;$WIN_WORKSPACE/build/src/vs2017_15.8.4/SDK/Include/10.0.17134.0/um;$WIN_WORKSPACE/build/src/vs2017_15.8.4/SDK/Include/10.0.17134.0/winrt;$WIN_WORKSPACE/build/src/vs2017_15.8.4/DIA SDK/include"

    export LIB="$WIN_WORKSPACE/build/src/vs2017_15.8.4/VC/lib/x64;$WIN_WORKSPACE/build/src/vs2017_15.8.4/VC/atlmfc/lib/x64;$WIN_WORKSPACE/build/src/vs2017_15.8.4/SDK/lib/10.0.17134.0/um/x64;$WIN_WORKSPACE/build/src/vs2017_15.8.4/SDK/lib/10.0.17134.0/ucrt/x64;$WIN_WORKSPACE/build/src/vs2017_15.8.4/DIA SDK/lib/amd64"

    PATH="$WORKSPACE/build/src/vs2017_15.8.4/VC/bin/Hostx64/x64:$WORKSPACE/build/src/vs2017_15.8.4/VC/bin/Hostx86/x86:$WORKSPACE/build/src/vs2017_15.8.4/SDK/bin/10.0.17134.0/x64:$WORKSPACE/build/src/vs2017_15.8.4/redist/x64/Microsoft.VC141.CRT:$WORKSPACE/build/src/vs2017_15.8.4/SDK/Redist/ucrt/DLLs/x64:$WORKSPACE/build/src/vs2017_15.8.4/DIA SDK/bin/amd64:$WORKSPACE/build/src/mingw64/bin:$PATH"

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
  -fuse-ld=$PWD/cctools/bin/x86_64-darwin11-ld \
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

# XXX On Windows there's a workspace/builds/src/Cargo.toml from the root of
# mozilla-central, and cargo complains below if it's not gone...
if [ -f Cargo.toml ]; then
  cat Cargo.toml
  rm Cargo.toml
fi

git clone https://github.com/eqrion/cbindgen cbindgen

cd $_

git checkout $CBINDGEN_REVISION

cargo build --verbose --release --target "$TARGET"

mkdir cbindgen
cp target/$TARGET/release/cbindgen* cbindgen/
tar -acf cbindgen.tar.$COMPRESS_EXT cbindgen
mkdir -p $UPLOAD_DIR
cp cbindgen.tar.$COMPRESS_EXT $UPLOAD_DIR
