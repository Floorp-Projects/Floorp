#!/bin/bash
# Compares two mars

marA="$1"
marB="$2"
testDir="$3"
workdir="/tmp/diffmar/$testDir"
fromdir="$workdir/0"
todir="$workdir/1"

# On Windows, creation time can be off by a second or more between the files in
# the fromdir and todir due to them being extracted synchronously so use
# time-style and exclude seconds from the creation time.
lsargs="-algR"
unamestr=`uname`
if [ ! "$unamestr" = 'Darwin' ]; then
  unamestr=`uname -o`
  if [ "$unamestr" = 'Msys' -o "$unamestr" = "Cygwin" ]; then
     lsargs="-algR --time-style=+%Y-%m-%d-%H:%M"
  fi
fi

rm -rf "$workdir"
mkdir -p "$fromdir"
mkdir -p "$todir"

cp "$1" "$fromdir"
cp "$2" "$todir"

cd "$fromdir"
mar -x "$1"
rm "$1"
mv updatev2.manifest updatev2.manifest.bz2
bzip2 -d updatev2.manifest.bz2
mv updatev3.manifest updatev3.manifest.bz2
bzip2 -d updatev3.manifest.bz2
ls $lsargs > files.txt

cd "$todir"
mar -x "$2"
rm "$2"
mv updatev2.manifest updatev2.manifest.bz2
bzip2 -d updatev2.manifest.bz2
mv updatev3.manifest updatev3.manifest.bz2
bzip2 -d updatev3.manifest.bz2
ls $lsargs > files.txt

echo "diffing $fromdir and $todir"
echo "on linux shell sort and python sort return different results"
echo "which can cause differences in the manifest files"
diff -ru "$fromdir" "$todir"
