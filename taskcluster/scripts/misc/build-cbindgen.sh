#!/bin/bash
set -x -e -v

CBINDGEN_VERSION=v0.6.1
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

    export INCLUDE="$WIN_WORKSPACE/build/src/vs2017_15.4.2/VC/include;$WIN_WORKSPACE/build/src/vs2017_15.4.2/VC/atlmfc/include;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/Include/10.0.15063.0/ucrt;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/Include/10.0.15063.0/shared;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/Include/10.0.15063.0/um;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/Include/10.0.15063.0/winrt;$WIN_WORKSPACE/build/src/vs2017_15.4.2/DIA SDK/include"

    export LIB="$WIN_WORKSPACE/build/src/vs2017_15.4.2/VC/lib/x64;$WIN_WORKSPACE/build/src/vs2017_15.4.2/VC/atlmfc/lib/x64;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/lib/10.0.15063.0/um/x64;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/lib/10.0.15063.0/ucrt/x64;$WIN_WORKSPACE/build/src/vs2017_15.4.2/DIA SDK/lib/amd64"

    PATH="$WORKSPACE/build/src/vs2017_15.4.2/VC/bin/Hostx64/x64:$WORKSPACE/build/src/vs2017_15.4.2/VC/bin/Hostx86/x86:$WORKSPACE/build/src/vs2017_15.4.2/SDK/bin/10.0.15063.0/x64:$WORKSPACE/build/src/vs2017_15.4.2/redist/x64/Microsoft.VC141.CRT:$WORKSPACE/build/src/vs2017_15.4.2/SDK/Redist/ucrt/DLLs/x64:$WORKSPACE/build/src/vs2017_15.4.2/DIA SDK/bin/amd64:$WORKSPACE/build/src/mingw64/bin:$PATH"
    ;;
esac

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh

PATH="$PWD/rustc/bin:$PATH"

# XXX On Windows there's a workspace/builds/src/Cargo.toml from the root of
# mozilla-central, and cargo complains below if it's not gone...
if [ -f Cargo.toml ]; then
  cat Cargo.toml
  rm Cargo.toml
fi

git clone https://github.com/eqrion/cbindgen cbindgen

cd $_

git checkout $CBINDGEN_VERSION

cargo build --verbose --release --target "$TARGET"

mkdir cbindgen
cp target/$TARGET/release/cbindgen* cbindgen/
tar -acf cbindgen.tar.$COMPRESS_EXT cbindgen
mkdir -p $UPLOAD_DIR
cp cbindgen.tar.$COMPRESS_EXT $UPLOAD_DIR
