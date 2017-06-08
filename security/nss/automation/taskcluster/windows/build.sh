#!/usr/bin/env bash

set -v -e -x

# Set up the toolchain.
if [ "$USE_64" = 1 ]; then
  source $(dirname $0)/setup64.sh
else
  source $(dirname $0)/setup32.sh
fi

# Clone NSPR.
hg_clone https://hg.mozilla.org/projects/nspr nspr default

# Build.
make -C nss nss_build_all

# Package.
7z a public/build/dist.7z dist
