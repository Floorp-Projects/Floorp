#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

# Clone NSPR if needed.
hg_clone https://hg.mozilla.org/projects/nspr ./nspr default

# Build.
nss/build.sh -g -v "$@"

# Package.
mkdir artifacts
tar cvfjh artifacts/dist.tar.bz2 dist
