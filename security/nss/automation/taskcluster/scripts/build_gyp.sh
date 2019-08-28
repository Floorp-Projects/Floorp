#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Clone NSPR if needed.
hg_clone https://hg.mozilla.org/projects/nspr ./nspr default

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
