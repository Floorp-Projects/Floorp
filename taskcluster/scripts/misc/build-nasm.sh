#!/bin/bash
set -x -e -v

COMPRESS_EXT=zst

cd $MOZ_FETCHES_DIR/nasm-*

case $(cat version) in
2.14.02)
        # Fix for .debug_loc section containing garbage on elf32
        # https://bugzilla.nasm.us/show_bug.cgi?id=3392631
        patch -p1 <<'EOF'
diff --git a/output/outelf.c b/output/outelf.c
index de99d076..47031e12 100644
--- a/output/outelf.c
+++ b/output/outelf.c
@@ -3275,7 +3275,7 @@ static void dwarf_generate(void)
     WRITELONG(pbuf,framelen-4); /* initial length */
 
     /* build loc section */
-    loclen = 16;
+    loclen = is_elf64() ? 16 : 8;
     locbuf = pbuf = nasm_malloc(loclen);
     if (is_elf32()) {
         WRITELONG(pbuf,0);  /* null  beginning offset */
EOF
	;;
esac

case "$1" in
    win64)
        export PATH="$MOZ_FETCHES_DIR/clang/bin:$PATH"
        ./configure CC=x86_64-w64-mingw32-clang AR=llvm-ar RANLIB=llvm-ranlib --host=x86_64-w64-mingw32
        EXE=.exe
        ;;
    macosx64)
        export PATH="$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/cctools/bin:$PATH"
        export LD_LIBRARY_PATH="$MOZ_FETCHES_DIR/clang/lib"
        export MACOSX_DEPLOYMENT_TARGET=10.12
        ./configure CC="clang --target=x86_64-apple-darwin -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk" --host=x86_64-apple-darwin
        EXE=
	;;
    macosx64-aarch64)
        export PATH="$MOZ_FETCHES_DIR/clang/bin:$MOZ_FETCHES_DIR/cctools/bin:$PATH"
        export LD_LIBRARY_PATH="$MOZ_FETCHES_DIR/clang/lib"
        export MACOSX_DEPLOYMENT_TARGET=11.0
        ./configure CC="clang --target=aarch64-apple-darwin -isysroot $MOZ_FETCHES_DIR/MacOSX11.3.sdk" --host=aarch64-apple-darwin
        EXE=
	;;
    *)
        ./configure CC="$MOZ_FETCHES_DIR/clang/bin/clang --sysroot=$MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu"
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
