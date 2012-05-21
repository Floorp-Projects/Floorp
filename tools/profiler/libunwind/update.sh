#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


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
