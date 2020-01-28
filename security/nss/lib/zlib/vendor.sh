#!/usr/bin/env bash
# Vendor a new version of zlib.
#
# Note: This script doesn't remove files if they are removed in the zlib release.

set -e

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <version>" 1>&2
    exit 2
fi

version="$1"
ztmp="zlib.$version"

cd "$(dirname "$0")"
../../fuzz/config/git-copy.sh https://github.com/madler/zlib "v$version" "$ztmp"
fullversion="$version ("$(cat "$ztmp"/.git-copy)")"
sed -i -e 's/^Version: .*/Version: '"$fullversion"'/' README.nss

prune=(
    .git-copy
    .gitignore
    CMakeLists.txt
    ChangeLog
    FAQ
    INDEX
    Makefile
    Makefile.in
    amiga
    configure
    contrib
    doc
    example.c
    examples
    make_vms.com
    minigzip.c
    msdos
    nintendods
    old
    os400
    qnx
    test
    treebuild.xml
    watcom
    win32
    zconf.h.cmakein
    zconf.h.in
    zlib.3
    zlib.3.pdf
    zlib.map
    zlib.pc.cmakein
    zlib.pc.in
    zlib2ansi
)
for i in "${prune[@]}"; do rm -rf "$ztmp"/"$i"; done
for i in "$ztmp"/*; do mv "$i" .; done
rmdir "$ztmp"
