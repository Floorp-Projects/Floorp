#!/bin/bash
# helper tool for testing.  Cats the manifest out of a mar file

mar="$1"
workdir="/tmp/catmanifest"

rm -rf "$workdir"
mkdir -p "$workdir"
cp "$1" "$workdir"
cd "$workdir"
mar -x "$1"
mv update.manifest update.manifest.bz2
bzip2 -d update.manifest.bz2
cat update.manifest
