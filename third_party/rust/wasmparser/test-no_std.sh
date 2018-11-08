#!/bin/bash
set -euo pipefail

# This is the test script for testing the no_std configuration of
# packages which support it.

# Repository top-level directory.
topdir=$(dirname "$0")
cd "$topdir"

function banner {
    echo "======  $*  ======"
}

banner "Rust unit tests"

# Test with just "core" enabled.
cargo +nightly test --no-default-features --features core

# Test with "core" and "std" enabled at the same time.
cargo +nightly test --features core

banner "OK"
