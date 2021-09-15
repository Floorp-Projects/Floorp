#!/bin/sh
set -x -e -v

cd "$MOZ_FETCHES_DIR"

# We want a lowercase bin directory and executable .exes to help configure
# on cross-compiles.
mv nsis/Bin nsis/bin
chmod +x nsis/bin/*.exe

tar caf nsis.tar.zst nsis
mkdir -p "$UPLOAD_DIR"
mv nsis.tar.zst "$UPLOAD_DIR"
