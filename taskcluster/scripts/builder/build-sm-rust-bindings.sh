#!/usr/bin/env bash

set -xe

source $(dirname $0)/sm-tooltool-config.sh

cd "$GECKO_PATH/js/rust"

cp $GECKO_PATH/.cargo/config.in $GECKO_PATH/.cargo/config

export RUSTFMT="$MOZ_FETCHES_DIR/rustc/bin/rustfmt"
export LD_LIBRARY_PATH="$MOZ_FETCHES_DIR/gcc/lib64"
# Enable backtraces if we panic.
export RUST_BACKTRACE=1

cargo test --verbose --frozen --features debugmozjs
cargo test --verbose --frozen
