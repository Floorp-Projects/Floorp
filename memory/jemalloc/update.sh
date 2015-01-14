#!/bin/bash

set -e

cd `dirname $0`

source upstream.info

rm -rf src
git clone "$UPSTREAM_REPO" src
cd src
git checkout "$UPSTREAM_COMMIT"
autoconf
git describe --long --abbrev=40 > VERSION
rm -rf .git .gitignore .gitattributes autom4te.cache .autom4te.cfg

patch -p1 < ../0001-Dont-overwrite-VERSION-on-a-git-repository.patch
patch -p1 < ../0002-Move-variable-declaration-to-the-top-its-block-for-M.patch
patch -p1 < ../0003-Add-a-isblank-definition-for-MSVC-2013.patch
patch -p1 < ../0004-Implement-stats.bookkeeping.patch
patch -p1 < ../0005-Bug-1121314-Avoid-needing-the-arena-in-chunk_alloc_d.patch

cd ..
hg addremove -q src

echo "jemalloc has now been updated.  Don't forget to run hg commit!"
