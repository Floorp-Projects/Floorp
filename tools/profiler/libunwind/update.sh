#!/bin/bash

set -e

cd `dirname $0`

source upstream.info

rm -rf src
git clone "$UPSTREAM_REPO" src
cd src
git checkout "$UPSTREAM_COMMIT"
autoreconf -i
rm -rf .git .gitignore autom4te.cache
cd ..
hg addremove -q src

echo "libunwind has now been updated.  Don't forget to run hg commit!"
