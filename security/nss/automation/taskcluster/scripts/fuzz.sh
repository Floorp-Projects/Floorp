#!/usr/bin/env bash

source $(dirname $0)/tools.sh

if [ $(id -u) = 0 ]; then
    # Drop privileges by re-running this script.
    exec su worker -c "$0 $*"
fi

# Fetch artifact if needed.
fetch_dist

# Clone corpus.
./nss/fuzz/clone_corpus.sh

# Fetch objdir name.
objdir=$(cat dist/latest)

# Run nssfuzz.
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:dist/$objdir/lib dist/$objdir/bin/nssfuzz $*
