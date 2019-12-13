#!/usr/bin/env bash

set -xe

source $(dirname $0)/sm-tooltool-config.sh

cd "$SRCDIR/js/rust"

cp $SRCDIR/.cargo/config.in $SRCDIR/.cargo/config

export RUSTFMT="$MOZ_FETCHES_DIR/rustc/bin/rustfmt"
export LD_LIBRARY_PATH="$MOZ_FETCHES_DIR/gcc/lib64"
# Enable backtraces if we panic.
export RUST_BACKTRACE=1

cargo test --verbose --frozen --features debugmozjs
cargo test --verbose --frozen
