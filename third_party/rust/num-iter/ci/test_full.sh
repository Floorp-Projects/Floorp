#!/bin/bash

set -ex

echo Testing num-iter on rustc ${TRAVIS_RUST_VERSION}

# num-iter should build and test everywhere.
cargo build --verbose
cargo test --verbose

# test `no_std`
cargo build --verbose --no-default-features
cargo test --verbose --no-default-features

# test `i128`
if [[ "$TRAVIS_RUST_VERSION" =~ ^(nightly|beta|stable)$ ]]; then
    cargo build --verbose --features=i128
    cargo test --verbose --features=i128
fi
