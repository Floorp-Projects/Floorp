#!/bin/bash

set -ex

echo Testing num-bigint on rustc ${TRAVIS_RUST_VERSION}

# num-bigint should build and test everywhere.
cargo build --verbose
cargo test --verbose

# It should build with minimal features too.
cargo build --no-default-features
cargo test --no-default-features

# Each isolated feature should also work everywhere.
for feature in rand rustc-serialize serde; do
  cargo build --verbose --no-default-features --features="$feature"
  cargo test --verbose --no-default-features --features="$feature"
done

# Downgrade serde and build test the 0.7.0 channel as well
cargo update -p serde --precise 0.7.0
cargo build --verbose --no-default-features --features "serde"
