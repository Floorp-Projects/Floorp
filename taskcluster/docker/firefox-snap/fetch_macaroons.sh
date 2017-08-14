#!/bin/bash

url=$1
config=~/.snapcraft/snapcraft.cfg

mkdir -p ~/.snapcraft
curl -s $url | \
    python -c 'import json, sys; a = json.load(sys.stdin); print a["secret"]["content"]' | \
    base64 -d > \
    $config
chmod 600 $config
