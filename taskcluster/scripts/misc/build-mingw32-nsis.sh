#!/bin/bash
set -x -e -v

INSTALL_DIR=$MOZ_FETCHES_DIR/nsis

mkdir -p $INSTALL_DIR

cd $MOZ_FETCHES_DIR

export PATH="$MOZ_FETCHES_DIR/binutils/bin:$MOZ_FETCHES_DIR/clang/bin:$PATH"

# Call.S, included from CallCPP.S contains directives that clang's integrated
# assembler doesn't understand.
cat <<'EOF' >$MOZ_FETCHES_DIR/clang/bin/i686-w64-mingw32-gcc
#!/bin/sh
case "$@" in
*/CallCPP.S)
  $(dirname $0)/i686-w64-mingw32-clang -fno-integrated-as "$@"
  ;;
*)
  $(dirname $0)/i686-w64-mingw32-clang "$@"
  ;;
esac
EOF

chmod +x $MOZ_FETCHES_DIR/clang/bin/i686-w64-mingw32-gcc
ln -s i686-w64-mingw32-clang++ $MOZ_FETCHES_DIR/clang/bin/i686-w64-mingw32-g++

# --------------

cd zlib-1.2.11
make -f win32/Makefile.gcc PREFIX=i686-w64-mingw32-

cd ../nsis-3.07-src
patch -p1 < $GECKO_PATH/build/win32/nsis-no-insert-timestamp.patch
patch -p1 < $GECKO_PATH/build/win32/nsis-no-underscore.patch
# --exclude-libs is not supported by lld, but is not required anyways
sed -i 's/ -Wl,--exclude-libs,msvcrt.a//' SCons/Config/gnu
# memcpy.c and memset.c are built with a C++ compiler so we need to
# avoid their symbols being mangled.
sed -i '2i extern "C"' SCons/Config/{memcpy,memset}.c
# Makensisw is skipped because its resource file fails to build with
# llvm-rc, but we don't need makensisw.
scons \
  PATH=$PATH \
  CC="clang --sysroot $MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu" \
  CXX="clang++ --sysroot $MOZ_FETCHES_DIR/sysroot-x86_64-linux-gnu" \
  XGCC_W32_PREFIX=i686-w64-mingw32- \
  ZLIB_W32=../zlib-1.2.11 \
  SKIPUTILS="NSIS Menu,Makensisw" \
  PREFIX_DEST=$INSTALL_DIR/ \
  PREFIX_BIN=bin \
  NSIS_CONFIG_CONST_DATA_PATH=no \
  VERSION=3.07 \
  install
# --------------

cd $MOZ_FETCHES_DIR

tar caf nsis.tar.zst nsis

mkdir -p $UPLOAD_DIR
cp nsis.tar.* $UPLOAD_DIR
