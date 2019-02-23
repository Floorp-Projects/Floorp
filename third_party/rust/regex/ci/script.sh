#!/bin/sh

# This is the main CI script for testing the regex crate and its sub-crates.

set -ex

# Builds the regex crate and runs tests.
cargo build --verbose
cargo doc --verbose

# Run tests. If we have nightly, then enable our nightly features.
# Right now there are no nightly features, but that may change in the
# future.
CARGO_TEST_EXTRA_FLAGS=""
if [ "$TRAVIS_RUST_VERSION" = "nightly" ]; then
  CARGO_TEST_EXTRA_FLAGS=""
fi
cargo test --verbose ${CARGO_TEST_EXTRA_FLAGS}

# Run the random tests in release mode, as this is faster.
RUST_REGEX_RANDOM_TEST=1 \
    cargo test --release --verbose \
    ${CARGO_TEST_EXTRA_FLAGS} --test crates-regex

# Run a test that confirms the shootout benchmarks are correct.
ci/run-shootout-test

# Run tests on regex-syntax crate.
cargo test --verbose --manifest-path regex-syntax/Cargo.toml
cargo doc --verbose --manifest-path regex-syntax/Cargo.toml

# Run tests on regex-capi crate.
ci/test-regex-capi

# Make sure benchmarks compile. Don't run them though because they take a
# very long time. Also, check that we can build the regex-debug tool.
if [ "$TRAVIS_RUST_VERSION" = "nightly" ]; then
  cargo build --verbose --manifest-path regex-debug/Cargo.toml
  for x in rust rust-bytes pcre1 onig; do
    (cd bench && ./run $x --no-run --verbose)
  done
fi
