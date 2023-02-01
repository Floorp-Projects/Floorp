#!/bin/bash

set -x
set -e
set -o pipefail

[ -z "$1" ] && echo Missing argument && exit 1

dir=$(dirname "$1")
name=$(basename "$1")

case "$(uname -s)" in
Darwin)
    TAR_FLAGS=--no-fflags
    ;;
*)
    TAR_FLAGS=
    ;;
esac

(cd "$dir"; find "$name"/* -not -type d -print0 | tar $TAR_FLAGS -cvf - --null -T -) | python3 $GECKO_PATH/taskcluster/scripts/misc/zstdpy > "$name.tar.zst"

mkdir -p "$UPLOAD_DIR"
mv "$name.tar.zst" "$UPLOAD_DIR"
