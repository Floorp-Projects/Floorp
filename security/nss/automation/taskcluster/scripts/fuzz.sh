#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

type="$1"
shift

# Fetch artifact if needed.
fetch_dist

# Clone corpus.
./nss/fuzz/clone_corpus.sh

# Ensure we have a directory.
mkdir -p nss/fuzz/corpus/$type

# Fetch objdir name.
objdir=$(cat dist/latest)

# Run nssfuzz.
dist/$objdir/bin/nssfuzz-"$type" "$@"
