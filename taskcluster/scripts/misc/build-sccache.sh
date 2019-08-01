#!/bin/bash
set -x -e -v

TARGET="$1"

# This script is for building sccache

case "$(uname -s)" in
Linux)
    WORKSPACE=$HOME/workspace
    COMPRESS_EXT=xz
    PATH="$WORKSPACE/build/src/binutils/bin:$PATH"
    ;;
MINGW*)
    WORKSPACE=$PWD
    UPLOAD_DIR=$WORKSPACE/public/build
    COMPRESS_EXT=bz2

    . $GECKO_PATH/taskcluster/scripts/misc/vs-setup.sh
    ;;
esac

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh

PATH="$PWD/rustc/bin:$PATH"

cd $MOZ_FETCHES_DIR/sccache

case "$(uname -s)" in
Linux)
    if [ "$TARGET" == "x86_64-apple-darwin" ]; then
        export PATH="$WORKSPACE/build/src/llvm-dsymutil/bin:$PATH"
        export PATH="$WORKSPACE/build/src/cctools/bin:$PATH"
        cat >$WORKSPACE/build/src/cross-linker <<EOF
exec $WORKSPACE/build/src/clang/bin/clang -v \
  -fuse-ld=$WORKSPACE/build/src/cctools/bin/x86_64-apple-darwin-ld \
  -mmacosx-version-min=10.11 \
  -target $TARGET \
  -B $WORKSPACE/build/src/cctools/bin \
  -isysroot $WORKSPACE/build/src/MacOSX10.11.sdk \
  "\$@"
EOF
        chmod +x $WORKSPACE/build/src/cross-linker
        export RUSTFLAGS="-C linker=$WORKSPACE/build/src/cross-linker"
        export CC="$WORKSPACE/build/src/clang/bin/clang"
        cargo build --features "all" --verbose --release --target $TARGET
    else
        # We can't use the system openssl; see the sad story in
        # https://bugzilla.mozilla.org/show_bug.cgi?id=1163171#c26.
        OPENSSL_BUILD_DIRECTORY=$PWD/ourssl
        pushd $MOZ_FETCHES_DIR/openssl-1.1.0g
        ./Configure --prefix=$OPENSSL_BUILD_DIRECTORY no-shared linux-x86_64
        make -j `nproc --all`
        # `make install` installs a *ton* of docs that we don't care about.
        # Just the software, please.
        make install_sw
        popd
        # We don't need to set OPENSSL_STATIC here, because we only have static
        # libraries in the directory we are passing.
        env "OPENSSL_DIR=$OPENSSL_BUILD_DIRECTORY" cargo build --features "all dist-server" --verbose --release
    fi

    ;;
MINGW*)
    cargo build --verbose --release --features="dist-client s3 gcs"
    ;;
esac

SCCACHE_OUT=target/release/sccache*
if [ -n "$TARGET" ]; then
    SCCACHE_OUT=target/$TARGET/release/sccache*
fi

mkdir sccache
cp $SCCACHE_OUT sccache/
tar -acf sccache.tar.$COMPRESS_EXT sccache
mkdir -p $UPLOAD_DIR
cp sccache.tar.$COMPRESS_EXT $UPLOAD_DIR
