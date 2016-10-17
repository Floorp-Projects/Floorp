#!/usr/bin/env bash

source $(dirname $0)/tools.sh

if [[ $(id -u) -eq 0 ]]; then
    # Drop privileges by re-running this script.
    exec su worker $0
fi

# Clone NSPR if needed.
hg_clone https://hg.mozilla.org/projects/nspr nspr default

# Build.
cd nss && NSS_GYP_GEN=1 ./build.sh
if [ $? != 0 ]; then
    exit 1
fi

# Package.
cd .. && mkdir artifacts
tar cvfjh artifacts/dist.tar.bz2 dist
