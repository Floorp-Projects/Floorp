#!/bin/bash
set -x -e -v

SCCACHE_REVISION=43300e1976bdbfc8dbda30e22a00ce2cce54e9de

# This script is for building sccache

case "$(uname -s)" in
Linux)
    WORKSPACE=$HOME/workspace
    UPLOAD_DIR=$HOME/artifacts
    export CC=clang
    PATH="$WORKSPACE/build/src/clang/bin:$PATH"
    COMPRESS_EXT=xz
    ;;
MINGW*)
    WORKSPACE=$PWD
    UPLOAD_DIR=$WORKSPACE/public/build
    WIN_WORKSPACE="$(pwd -W)"
    COMPRESS_EXT=bz2

    export INCLUDE="$WIN_WORKSPACE/build/src/vs2015u3/VC/include;$WIN_WORKSPACE/build/src/vs2015u3/VC/atlmfc/include;$WIN_WORKSPACE/build/src/vs2015u3/SDK/Include/10.0.14393.0/ucrt;$WIN_WORKSPACE/build/src/vs2015u3/SDK/Include/10.0.14393.0/shared;$WIN_WORKSPACE/build/src/vs2015u3/SDK/Include/10.0.14393.0/um;$WIN_WORKSPACE/build/src/vs2015u3/SDK/Include/10.0.14393.0/winrt;$WIN_WORKSPACE/build/src/vs2015u3/DIA SDK/include"

    export LIB="$WIN_WORKSPACE/build/src/vs2015u3/VC/lib/amd64;$WIN_WORKSPACE/build/src/vs2015u3/VC/atlmfc/lib/amd64;$WIN_WORKSPACE/build/src/vs2015u3/SDK/lib/10.0.14393.0/um/x64;$WIN_WORKSPACE/build/src/vs2015u3/SDK/lib/10.0.14393.0/ucrt/x64;$WIN_WORKSPACE/build/src/vs2015u3/DIA SDK/lib/amd64"

    PATH="$WORKSPACE/build/src/vs2015u3/VC/bin/amd64:$WORKSPACE/build/src/vs2015u3/VC/bin:$WORKSPACE/build/src/vs2015u3/SDK/bin/x64:$WORKSPACE/build/src/vs2015u3/redist/x64/Microsoft.VC140.CRT:$WORKSPACE/build/src/vs2015u3/SDK/Redist/ucrt/DLLs/x64:$WORKSPACE/build/src/vs2015u3/DIA SDK/bin/amd64:$WORKSPACE/build/src/mingw64/bin:$PATH"
    ;;
esac

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh

PATH="$PWD/rustc/bin:$PATH"

git clone https://github.com/mozilla/sccache sccache

cd sccache

git checkout $SCCACHE_REVISION

cargo build --verbose --release

mkdir sccache2
cp target/release/sccache* sccache2/
tar -acf sccache2.tar.$COMPRESS_EXT sccache2
mkdir -p $UPLOAD_DIR
cp sccache2.tar.$COMPRESS_EXT $UPLOAD_DIR
