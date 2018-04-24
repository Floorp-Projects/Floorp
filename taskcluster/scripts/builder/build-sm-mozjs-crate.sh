#!/usr/bin/env bash

set -xe

source $(dirname $0)/sm-tooltool-config.sh

# Ensure that we have a .config/cargo that points us to our vendored crates
# rather than to crates.io.
cd "$SRCDIR/.cargo"
sed -e "s|@top_srcdir@|$SRCDIR|" -e 's|@[^@]*@||g' < config.in > config

cd "$SRCDIR/js/src"

export PATH="$PATH:$TOOLTOOL_CHECKOUT/cargo/bin:$TOOLTOOL_CHECKOUT/rustc/bin"
export RUST_BACKTRACE=1
export AUTOMATION=1

cargo build --verbose --frozen --features debugmozjs
cargo build --verbose --frozen
