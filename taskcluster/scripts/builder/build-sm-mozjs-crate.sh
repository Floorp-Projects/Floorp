#!/usr/bin/env bash

set -xe

source $(dirname $0)/sm-tooltool-config.sh

cd "$GECKO_PATH/js/src"

cp $GECKO_PATH/.cargo/config.in $GECKO_PATH/.cargo/config

export PATH="$PATH:$MOZ_FETCHES_DIR/cargo/bin:$MOZ_FETCHES_DIR/rustc/bin"
export RUSTFMT="$MOZ_FETCHES_DIR/rustc/bin/rustfmt"
export RUST_BACKTRACE=1
export AUTOMATION=1

cargo build --verbose --frozen --features debugmozjs
cargo build --verbose --frozen
