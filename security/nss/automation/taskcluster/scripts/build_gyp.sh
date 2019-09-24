#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Clone NSPR if needed.
hg_clone https://hg.mozilla.org/projects/nspr ./nspr default

if [[ -f nss/nspr.patch && "$ALLOW_NSPR_PATCH" == "1" ]]; then
  pushd nspr
  cat ../nss/nspr.patch | patch -p1
  popd
fi

# Build.
nss/build.sh -g -v --enable-libpkix "$@"

# Package.
if [[ $(uname) = "Darwin" ]]; then
  mkdir -p public
  tar cvfjh public/dist.tar.bz2 dist
else
  mkdir artifacts
  tar cvfjh artifacts/dist.tar.bz2 dist
fi
