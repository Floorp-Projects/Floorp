#!/usr/bin/env bash

set -xe

source $(dirname $0)/sm-tooltool-config.sh

# Ensure that we have a .config/cargo that points us to our vendored crates
# rather than to crates.io.
cd "$SRCDIR/.cargo"
sed -e "s|@top_srcdir@|$SRCDIR|" -e 's|@[^@]*@||g' < config.in > config

cd "$SRCDIR/js/rust"

# Enable backtraces if we panic.
export RUST_BACKTRACE=1

cargo test --verbose --frozen --features debugmozjs
cargo test --verbose --frozen
