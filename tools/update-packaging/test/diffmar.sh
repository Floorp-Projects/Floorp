#!/bin/bash
# Compares two mars

marA="$1"
marB="$2"
workdir="/tmp/diffmar"
fromdir="$workdir/0"
todir="$workdir/1"

rm -rf "$workdir"
mkdir -p "$fromdir"
mkdir -p "$todir"

cp "$1" "$fromdir"
cp "$2" "$todir"

cd "$fromdir"
mar -x "$1"
rm "$1"
mv update.manifest update.manifest.bz2
bzip2 -d update.manifest.bz2
ls -algR > files.txt
# Sort the manifest so we don't get any diffs for ordering
#cat update.manifest | sort > update.manifest

cd "$todir"
mar -x "$2"
rm "$2"
mv update.manifest update.manifest.bz2
bzip2 -d update.manifest.bz2
# Sort the manifest so we don't get any diffs for ordering
#cat update.manifest | sort > update.manifest
ls -algR > files.txt

diff -r "$fromdir" "$todir"