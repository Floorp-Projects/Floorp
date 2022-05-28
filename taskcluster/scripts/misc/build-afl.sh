#!/bin/sh

set -e -x

artifact=$(basename "$TOOLCHAIN_ARTIFACT")
dir=${artifact%.tar.*}
scripts="$(realpath "${0%/*}")"

cd "$MOZ_FETCHES_DIR/AFLplusplus"
make -f GNUmakefile afl-showmap \
    CC="$MOZ_FETCHES_DIR/clang/bin/clang"
make -f GNUmakefile.llvm install \
    DESTDIR="$dir" \
    PREFIX=/ \
    LLVM_CONFIG="$MOZ_FETCHES_DIR/clang/bin/llvm-config"
rm -rf "$dir/share"

tar caf "$artifact" "$dir"

mkdir -p "$UPLOAD_DIR"
mv "$artifact" "$UPLOAD_DIR"
