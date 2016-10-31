#!/usr/bin/env bash

source $(dirname $0)/tools.sh

if [[ $(id -u) -eq 0 ]]; then
    # Drop privileges by re-running this script.
    exec su worker $0
fi

# Clone NSPR if needed.
hg_clone https://hg.mozilla.org/projects/nspr nspr default

# Build.
nss/build.sh -g -v

# Package.
mkdir artifacts
tar cvfjh artifacts/dist.tar.bz2 dist
