#!/bin/sh

# This is the main CI script for testing the regex crate and its sub-crates.

set -ex

# Builds the regex crate and runs tests.
cargo build --verbose
cargo doc --verbose

# Run tests. If we have nightly, then enable our nightly features.
if [ "$TRAVIS_RUST_VERSION" = "nightly" ]; then
  cargo test --verbose --features unstable
else
  cargo test --verbose
fi

# Run a test that confirms the shootout benchmarks are correct.
ci/run-shootout-test

# Run tests on regex-syntax crate.
cargo test --verbose --manifest-path regex-syntax/Cargo.toml
cargo doc --verbose --manifest-path regex-syntax/Cargo.toml

# Run tests on regex-capi crate.
cargo build --verbose --manifest-path regex-capi/Cargo.toml
(cd regex-capi/ctest && ./compile && LD_LIBRARY_PATH=../../target/debug ./test)
(cd regex-capi/examples && ./compile && LD_LIBRARY_PATH=../../target/debug ./iter)

# Make sure benchmarks compile. Don't run them though because they take a
# very long time. Also, check that we can build the regex-debug tool.
if [ "$TRAVIS_RUST_VERSION" = "nightly" ]; then
  cargo build --verbose --manifest-path regex-debug/Cargo.toml
  for x in rust rust-bytes pcre1 onig; do
    (cd bench && ./run $x --no-run --verbose)
  done
fi
