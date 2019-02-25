#!/bin/bash
set -x -e -v

case "$(uname -s)" in
    Linux)
        WORKSPACE=$HOME/workspace
        UPLOAD_DIR=$HOME/artifacts
        COMPRESS_EXT=bz2
        ;;
esac

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh
export PATH="$WORKSPACE/build/src/clang/bin:$PATH"

cd $WORKSPACE/build/nasm-2.14.02
./configure CC=x86_64-w64-mingw32-clang AR=llvm-ar RANLIB=llvm-ranlib --host=x86_64-w64-mingw32
make -j$(nproc)

mkdir nasm
mv nasm.exe nasm/
tar -acf nasm.tar.$COMPRESS_EXT nasm
mkdir -p "$UPLOAD_DIR"
cp nasm.tar.$COMPRESS_EXT "$UPLOAD_DIR"
