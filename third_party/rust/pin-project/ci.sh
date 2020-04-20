#!/bin/bash

# A script to run a simplified version of the checks done by CI.
#
# Usage
#
# ```sh
# . ./ci.sh
# ```

echo "Running 'cargo fmt -- --check'"
cargo +nightly fmt --all -- --check

echo "Running 'cargo clippy'"
cargo +nightly clippy --all --all-features

echo "Running 'cargo test'"
cargo +nightly test --all --all-features

echo "Running 'cargo doc'"
cargo +nightly doc --no-deps --all --all-features

echo "Running 'compiletest'"
. ./compiletest.sh
