#!/bin/bash

set -ex

url="$1"

CONFIG="$HOME/.config/snapcraft/snapcraft.cfg"

mkdir -p "$( dirname "$CONFIG" )"
curl -s "$url" | \
    python -c 'import json, sys; a = json.load(sys.stdin); print a["secret"]["content"]' | \
    base64 -d > \
    "$CONFIG"
chmod 600 "$CONFIG"
