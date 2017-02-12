#!/bin/sh

# This is the main CI script for testing the regex crate and its sub-crates.

set -e

# Builds the regex crate and runs tests.
cargo build --verbose
cargo doc --verbose
if [ "$TRAVIS_RUST_VERSION" = "nightly" ]; then
  cargo build --verbose --manifest-path regex-debug/Cargo.toml
  RUSTFLAGS="-C target-feature=+ssse3" cargo test --verbose --features 'simd-accel pattern' --jobs 4
else
  cargo test --verbose --jobs 4
fi

# Run a test that confirms the shootout benchmarks are correct.
ci/run-shootout-test

# Run tests on regex-syntax crate.
cargo test --verbose --manifest-path regex-syntax/Cargo.toml
cargo doc --verbose --manifest-path regex-syntax/Cargo.toml

# Run tests on regex-capi crate.
cargo build --verbose --manifest-path regex-capi/Cargo.toml
(cd regex-capi/ctest && ./compile && LD_LIBRARY_PATH=../target/debug ./test)
(cd regex-capi/examples && ./compile && LD_LIBRARY_PATH=../target/debug ./iter)

# Make sure benchmarks compile. Don't run them though because they take a
# very long time.
if [ "$TRAVIS_RUST_VERSION" = "nightly" ]; then
  for x in rust rust-bytes pcre1 onig; do
    (cd bench && ./run $x --no-run)
  done
fi
