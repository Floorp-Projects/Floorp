#!/bin/bash
set -x -e -v

# 0.2.2
SCCACHE_REVISION=f5d7bac801a32734f4bf351edd6ae9a539424839

# This script is for building sccache

case "$(uname -s)" in
Linux)
    WORKSPACE=$HOME/workspace
    UPLOAD_DIR=$HOME/artifacts
    export CC=gcc
    PATH="$WORKSPACE/build/src/gcc/bin:$PATH"
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

# Link openssl statically so we don't have to bother with different sonames
# across Linux distributions.  We can't use the system openssl; see the sad
# story in https://bugzilla.mozilla.org/show_bug.cgi?id=1163171#c26.
case "$(uname -s)" in
Linux)
    OPENSSL_TARBALL=openssl-1.1.0f.tar.gz

    curl -O https://www.openssl.org/source/$OPENSSL_TARBALL
cat >$OPENSSL_TARBALL.sha256sum <<EOF
12f746f3f2493b2f39da7ecf63d7ee19c6ac9ec6a4fcd8c229da8a522cb12765  openssl-1.1.0f.tar.gz
EOF
    cat $OPENSSL_TARBALL.sha256sum
    sha256sum -c $OPENSSL_TARBALL.sha256sum

    tar zxf $OPENSSL_TARBALL

    OPENSSL_BUILD_DIRECTORY=$PWD/ourssl
    pushd $(basename $OPENSSL_TARBALL .tar.gz)
    ./Configure --prefix=$OPENSSL_BUILD_DIRECTORY no-shared linux-x86_64
    make -j `nproc --all`
    # `make install` installs a *ton* of docs that we don't care about.
    # Just the software, please.
    make install_sw
    popd

    # We don't need to set OPENSSL_STATIC here, because we only have static
    # libraries in the directory we are passing.
    env "OPENSSL_DIR=$OPENSSL_BUILD_DIRECTORY" cargo build --verbose --release
    ;;
MINGW*)
    cargo build --verbose --release
    ;;
esac

mkdir sccache2
cp target/release/sccache* sccache2/
tar -acf sccache2.tar.$COMPRESS_EXT sccache2
mkdir -p $UPLOAD_DIR
cp sccache2.tar.$COMPRESS_EXT $UPLOAD_DIR
