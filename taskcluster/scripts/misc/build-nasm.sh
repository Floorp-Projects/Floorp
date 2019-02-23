
#!/bin/bash
set -x -e -v

WORKSPACE=$HOME/workspace
UPLOAD_DIR=$HOME/artifacts
COMPRESS_EXT=bz2

cd $WORKSPACE/build/src

. taskcluster/scripts/misc/tooltool-download.sh

cd $WORKSPACE/build/nasm-*
case "$1" in
    win64)
        export PATH="$WORKSPACE/build/src/clang/bin:$PATH"
        ./configure CC=x86_64-w64-mingw32-clang AR=llvm-ar RANLIB=llvm-ranlib --host=x86_64-w64-mingw32
        EXE=.exe
        ;;
    *)
        ./configure
        EXE=
        ;;
esac
make -j$(nproc)

mv nasm$EXE nasm-tmp
mkdir nasm
mv nasm-tmp nasm/nasm$EXE
tar -acf nasm.tar.$COMPRESS_EXT nasm
mkdir -p "$UPLOAD_DIR"
cp nasm.tar.$COMPRESS_EXT "$UPLOAD_DIR"
