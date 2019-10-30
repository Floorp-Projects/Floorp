#!/usr/bin/env bash

set -ex

export RUST_TEST_THREADS=1
export RUST_BACKTRACE=1
export RUST_TEST_NOCAPTURE=1

export RUSTFLAGS="${RUSTFLAGS} -Z sanitizer=${SANITIZER}"
export ASAN_OPTIONS="detect_odr_violation=0:detect_leaks=0"

export OPT="--target=x86_64-unknown-linux-gnu"
export OPT_RELEASE="--release ${OPT}"

if [[ $SANITIZER == "memory" ]]; then
    export RUSTFLAGS="${RUSTFLAGS} -C panic=abort"
fi

if [[ $SANITIZER == "address" ]]; then
    cargo test --lib $OPT
    cargo test --lib $OPT_RELEASE
fi

cd san
cargo run $OPT
cargo run $OPT_RELEASE
