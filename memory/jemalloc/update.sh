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
rm -rf .git .gitignore autom4te.cache
cd ..
hg addremove -q src

echo "jemalloc has now been updated.  Don't forget to run hg commit!"
