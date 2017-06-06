#!/usr/bin/env bash

set -v -e -x

# Set up the toolchain.
source $(dirname $0)/setup.sh

# Clone NSPR.
hg_clone https://hg.mozilla.org/projects/nspr nspr default

# Build.
make -C nss nss_build_all

# Package.
7z a public/build/dist.7z dist
