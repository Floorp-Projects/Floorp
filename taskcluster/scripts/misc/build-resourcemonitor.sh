#!/bin/bash
set -x -e -v

cd "$MOZ_FETCHES_DIR"/resource-monitor/ || exit 1
COMPRESS_EXT=zst

PATH="$MOZ_FETCHES_DIR/go/bin:$PATH"
export PATH

EXE_SUFFIX=""

case "$1" in
    linux64)   GOOS=linux; GOARCH=amd64 ;;
    macos64)   GOOS=darwin; GOARCH=amd64 ;;
    windows64) GOOS=windows; GOARCH=amd64; EXE_SUFFIX=".exe" ;;
    windows32) GOOS=windows; GOARCH=386;   EXE_SUFFIX=".exe" ;;
    *)
        echo "Unknown architecture $1 not recognized in build-resourcemonitor.sh" >&2
        exit 1
    ;;
esac

export GOOS
export GOARCH
export EXE_SUFFIX

echo "GOOS=$GOOS"
echo "GOARCH=$GOARCH"

go build .

STAGING_DIR="resource-monitor"
mv "resource-monitor${EXE_SUFFIX}" resource-monitor.tmp
mkdir "${STAGING_DIR}"

cp resource-monitor.tmp "${STAGING_DIR}/resource-monitor${EXE_SUFFIX}"

tar -acf "resource-monitor.tar.$COMPRESS_EXT" "${STAGING_DIR}"/
mkdir -p "$UPLOAD_DIR"
cp "resource-monitor.tar.$COMPRESS_EXT" "$UPLOAD_DIR"
