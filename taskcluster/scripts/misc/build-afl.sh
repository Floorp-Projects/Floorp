#!/bin/sh

set -e -x

artifact="$(basename "$TOOLCHAIN_ARTIFACT")"
dir="${artifact%.tar.*}"
scripts="$(realpath "${0%/*}")"

cd "$MOZ_FETCHES_DIR/AFLplusplus"
patch -p1 -i "$scripts/afl-nyx.patch"
make -f GNUmakefile afl-showmap \
    CC="$MOZ_FETCHES_DIR/clang/bin/clang"
make -f GNUmakefile.llvm install \
    CPPFLAGS="--sysroot $MOZ_FETCHES_DIR/sysroot" \
    DESTDIR="$dir" \
    LLVM_CONFIG="$MOZ_FETCHES_DIR/clang/bin/llvm-config" \
    PREFIX=/
rm -rf "$dir/share"

tar caf "$artifact" "$dir"

mkdir -p "$UPLOAD_DIR"
mv "$artifact" "$UPLOAD_DIR"
