#!/usr/bin/env bash

source $(dirname "$0")/tools.sh

type="$1"
shift

# Fetch artifact if needed.
fetch_dist

# Clone corpus.
./nss/fuzz/config/clone_corpus.sh

# Ensure we have a corpus.
if [ ! -d "nss/fuzz/corpus/$type" ]; then
  mkdir -p nss/fuzz/corpus/$type

  set +x

  # Create a corpus out of what we have.
  for f in $(find nss/fuzz/corpus -type f); do
    cp $f "nss/fuzz/corpus/$type"
  done

  set -x
fi

# Fetch objdir name.
objdir=$(cat dist/latest)

# Run nssfuzz.
dist/$objdir/bin/nssfuzz-"$type" "$@"
