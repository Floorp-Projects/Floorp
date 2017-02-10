#!/usr/bin/env bash

set -xeu
cd "$(dirname "$0")/.."

# Ensure we have the most up-to-date `rustfmt`.
cargo install -f rustfmt

# Run `rustfmt` on the crate! If `rustfmt` can't make a long line shorter, it
# prints an error and exits non-zero, so tell it to kindly shut its yapper and
# make sure it doesn't cause us to exit this whole script non-zero.
cargo fmt --quiet || true

# Exit non-zero if this resulted in any diffs.
./ci/assert-no-diff.sh

