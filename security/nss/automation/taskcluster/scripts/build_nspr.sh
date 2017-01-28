#!/usr/bin/env bash

set -v -e -x

source $(dirname $0)/tools.sh

# Clone NSPR if needed.
hg_clone https://hg.mozilla.org/projects/nspr nspr default

# Build.
rm -rf dist
make -C nss build_nspr

# Package.
test -d artifacts || mkdir artifacts
rm -rf dist-nspr
mv dist dist-nspr
tar cvfjh artifacts/dist-nspr.tar.bz2 dist-nspr
