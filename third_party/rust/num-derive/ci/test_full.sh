#!/bin/bash

set -ex

echo Testing num-derive on rustc ${TRAVIS_RUST_VERSION}

# num-derive should build and test everywhere.
cargo build --verbose --features="$FEATURES"
cargo test --verbose --features="$FEATURES"
