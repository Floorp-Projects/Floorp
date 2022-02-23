#!/bin/bash
set -x -e -v

shopt -s nullglob

# This script is for repacking clang for cross targets on a Linux host.

cd $MOZ_FETCHES_DIR

# We have a clang toolchain in $MOZ_FETCHES_DIR/clang
# We have some compiler-rts in $MOZ_FETCHES_DIR/compiler-rt*
# We copy everything from the compiler-rts into clang/lib/clang/$version
clang_dir=$(echo clang/lib/clang/*/include)
clang_dir=${clang_dir%/include}
[ -n "$clang_dir" ] && for c in compiler-rt*; do
  find $c -mindepth 1 -type d | while read d; do
    mkdir -p "$clang_dir/${d#compiler-rt-*/}"
    find $d -mindepth 1 -maxdepth 1 -not -type d | while read f; do
      target_file="$clang_dir/${f#compiler-rt-*/}"
      case $d in
      compiler-rt-*/lib/darwin)
        if [ -f "$target_file" ]; then
          # Unify overlapping files for darwin/
          $MOZ_FETCHES_DIR/cctools/bin/lipo -create "$f" "$target_file" -output "$target_file.new"
          mv "$target_file.new" "$target_file"
          continue
        fi
        ;;
      esac
      if [ -f "$target_file" ] && ! diff -q "$f" "$target_file" 2>/dev/null; then
        echo "Cannot copy $f because it is already in ${target_file%/*}" >&2 && exit 1
      fi
      cp "$f" "$target_file"
    done
  done
done

if [ -n "$UPLOAD_DIR" ]; then
  tar caf clang.tar.zst clang
  mkdir -p $UPLOAD_DIR
  mv clang.tar.zst $UPLOAD_DIR
fi
