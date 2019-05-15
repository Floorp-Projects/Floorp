#!/bin/bash
set -x -e -v

# 0.2.8 + a number of changes
SCCACHE_REVISION=5cbd30684e03cab9c2d1272fdc530fd83b8c903b

# This script is for building sccache

case "$(uname -s)" in
Linux)
    WORKSPACE=$HOME/workspace
    UPLOAD_DIR=$HOME/artifacts
    COMPRESS_EXT=xz
    PATH="$WORKSPACE/build/src/binutils/bin:$PATH"
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

PATH="$PWD/rustc/bin:$PATH"

git clone https://github.com/mozilla/sccache sccache

cd sccache

git checkout $SCCACHE_REVISION

# Link openssl statically so we don't have to bother with different sonames
# across Linux distributions.  We can't use the system openssl; see the sad
# story in https://bugzilla.mozilla.org/show_bug.cgi?id=1163171#c26.
case "$(uname -s)" in
Linux)
    OPENSSL_TARBALL=openssl-1.1.0g.tar.gz

    curl -L -O https://www.openssl.org/source/$OPENSSL_TARBALL
cat >$OPENSSL_TARBALL.sha256sum <<EOF
de4d501267da39310905cb6dc8c6121f7a2cad45a7707f76df828fe1b85073af  openssl-1.1.0g.tar.gz
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
    env "OPENSSL_DIR=$OPENSSL_BUILD_DIRECTORY" cargo build --features "all dist-server" --verbose --release
    ;;
MINGW*)
    cargo build --verbose --release --features="dist-client s3 gcs"
    ;;
esac

mkdir sccache2
cp target/release/sccache* sccache2/
tar -acf sccache2.tar.$COMPRESS_EXT sccache2
mkdir -p $UPLOAD_DIR
cp sccache2.tar.$COMPRESS_EXT $UPLOAD_DIR
