#!/bin/bash -vex

set -x -e

# Inputs, with defaults

: RUST_REPOSITORY ${RUST_REPOSITORY:=https://github.com/rust-lang/rust}
: RUST_BRANCH     ${RUST_BRANCH:=stable}

: WORKSPACE       ${WORKSPACE:=/home/worker}

set -v

# Check out rust sources
git clone $RUST_REPOSITORY -b $RUST_BRANCH ${WORKSPACE}/rust
