#!/bin/bash
set -x -e -v

# This script is for building grcov

OWNER=marco-c
PROJECT=grcov
PROJECT_REVISION=9214a916805838265764f9c69eaed657ea3db021

# This script is for building rust-size
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
    ;;
esac

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh

# cargo gets mad if the parent directory has a Cargo.toml file in it
if [ -e Cargo.toml ]; then
  mv Cargo.toml Cargo.toml.back
fi

PATH="$PWD/rustc/bin:$PATH"

git clone -n https://github.com/${OWNER}/${PROJECT} ${PROJECT}

pushd $PROJECT

git checkout $PROJECT_REVISION

cargo build --verbose --release

mkdir $PROJECT
cp target/release/${PROJECT}* ${PROJECT}/
pushd $PROJECT
tar -acf ../${PROJECT}.tar.$COMPRESS_EXT *
popd
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.$COMPRESS_EXT $UPLOAD_DIR

popd
if [ -e Cargo.toml.back ]; then
  mv Cargo.toml.back Cargo.toml
fi
