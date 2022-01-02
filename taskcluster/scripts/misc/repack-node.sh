#!/bin/bash
set -x -e -v

# This script is for repacking Node (and NPM) from nodejs.org.

mkdir -p "$UPLOAD_DIR"

cd "$MOZ_FETCHES_DIR"

# npx doesn't have great security characteristics (it downloads and executes
# stuff directly out of npm at runtime), so let's not risk it getting into
# anyone's PATH who doesn't already have it there:
rm -f node/bin/npx node/bin/npx.exe
tar caf "$UPLOAD_DIR"/node.tar.zst node
