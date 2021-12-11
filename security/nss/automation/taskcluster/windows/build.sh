#!/usr/bin/env bash

set -v -e -x

if [[ "$USE_64" == 1 ]]; then
    m=x64
else
    m=x86
fi
source "$(dirname "$0")/setup.sh"

# Clone NSPR.
hg_clone https://hg.mozilla.org/projects/nspr nspr default

if [[ -f nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
  pushd nspr
  cat ../nss/nspr.patch | patch -p1
  popd
fi

# Build.
make -C nss nss_build_all

# Package.
7z a public/build/dist.7z dist
