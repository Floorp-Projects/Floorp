#!/bin/sh

set -e -x

artifact=$(basename "$TOOLCHAIN_ARTIFACT")
dir="${artifact%.tar.*}"
scripts="$(realpath "${0%/*}")"

cd "$MOZ_FETCHES_DIR/AFL"
patch -p1 -i "$scripts/afl-nyx.patch"
make afl-showmap \
    CC="$MOZ_FETCHES_DIR/clang/bin/clang"
make -C llvm_mode install \
    DESTDIR="../$dir" \
    PREFIX=/ \
    LLVM_CONFIG="$MOZ_FETCHES_DIR/clang/bin/llvm-config"

tar caf "$artifact" "$dir"

mkdir -p "$UPLOAD_DIR"
mv "$artifact" "$UPLOAD_DIR"
