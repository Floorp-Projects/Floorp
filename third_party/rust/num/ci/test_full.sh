#!/bin/bash

set -ex

echo Testing num on rustc ${TRAVIS_RUST_VERSION:=$1}

# All of these packages should build and test everywhere.
for package in bigint complex integer iter rational traits; do
  cargo build --manifest-path $package/Cargo.toml
  cargo test --manifest-path $package/Cargo.toml
done

# Each isolated feature should also work everywhere.
for feature in '' bigint rational complex; do
  cargo build --verbose --no-default-features --features="$feature"
  cargo test --verbose --no-default-features --features="$feature"
done

# Build test for the serde feature
cargo build --verbose --features "serde"

# Downgrade serde and build test the 0.7.0 channel as well
cargo update -p serde --precise 0.7.0
cargo build --verbose --features "serde"


if [ "$TRAVIS_RUST_VERSION" = 1.8.0 ]; then exit; fi

# num-derive should build on 1.15.0+
cargo build --verbose --manifest-path=derive/Cargo.toml


if [ "$TRAVIS_RUST_VERSION" != nightly ]; then exit; fi

# num-derive testing requires compiletest_rs, which requires nightly
cargo test --verbose --manifest-path=derive/Cargo.toml

# num-macros only works on nightly, soon to be deprecated
cargo build --verbose --manifest-path=macros/Cargo.toml
cargo test --verbose --manifest-path=macros/Cargo.toml

# benchmarks only work on nightly
cargo bench --verbose
