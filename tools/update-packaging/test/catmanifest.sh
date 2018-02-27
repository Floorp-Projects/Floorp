#!/bin/bash
# helper tool for testing.  Cats the manifest out of a mar file

mar="$1"
workdir="/tmp/catmanifest"

rm -rf "$workdir"
mkdir -p "$workdir"
cp "$mar" "$workdir"
cd "$workdir" || exit 1
mar -x "$mar"
mv updatev2.manifest updatev2.manifest.xz
xz -d updatev2.manifest.xz
cat updatev2.manifest
