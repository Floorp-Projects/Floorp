#!/bin/bash
set -x -e -v

OWNER=luser
PROJECT=rust-size
PROJECT_REVISION=4a5d9148f50dc037dc230f10b8fc4e5ca00016aa

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

    export INCLUDE="$WIN_WORKSPACE/build/src/vs2017_15.4.2/VC/include;$WIN_WORKSPACE/build/src/vs2017_15.4.2/VC/atlmfc/include;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/Include/10.0.15063.0/ucrt;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/Include/10.0.15063.0/shared;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/Include/10.0.15063.0/um;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/Include/10.0.15063.0/winrt;$WIN_WORKSPACE/build/src/vs2017_15.4.2/DIA SDK/include"

    export LIB="$WIN_WORKSPACE/build/src/vs2017_15.4.2/VC/lib/x64;$WIN_WORKSPACE/build/src/vs2017_15.4.2/VC/atlmfc/lib/x64;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/lib/10.0.15063.0/um/x64;$WIN_WORKSPACE/build/src/vs2017_15.4.2/SDK/lib/10.0.15063.0/ucrt/x64;$WIN_WORKSPACE/build/src/vs2017_15.4.2/DIA SDK/lib/amd64"

    PATH="$WORKSPACE/build/src/vs2017_15.4.2/VC/bin/Hostx64/x64:$WORKSPACE/build/src/vs2017_15.4.2/VC/bin/Hostx86/x86:$WORKSPACE/build/src/vs2017_15.4.2/SDK/bin/10.0.15063.0/x64:$WORKSPACE/build/src/vs2017_15.4.2/redist/x64/Microsoft.VC141.CRT:$WORKSPACE/build/src/vs2017_15.4.2/SDK/Redist/ucrt/DLLs/x64:$WORKSPACE/build/src/vs2017_15.4.2/DIA SDK/bin/amd64:$WORKSPACE/build/src/mingw64/bin:$PATH"
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

cd $PROJECT

git checkout $PROJECT_REVISION

cargo build --verbose --release

mkdir $PROJECT
cp target/release/${PROJECT}* ${PROJECT}/
tar -acf ${PROJECT}.tar.$COMPRESS_EXT $PROJECT
mkdir -p $UPLOAD_DIR
cp ${PROJECT}.tar.$COMPRESS_EXT $UPLOAD_DIR

cd ..
if [ -e Cargo.toml.back ]; then
  mv Cargo.toml.back Cargo.toml
fi
