#!/bin/sh
# Use rustup to locally run the same suite of tests as .travis.yml.
# (You should first install/update 1.8.0, 1.15.0, beta, and nightly.)

set -ex

for toolchain in 1.8.0 1.15.0 beta nightly; do
    run="rustup run $toolchain"
    $run cargo build --verbose
    $run $PWD/ci/test_full.sh $toolchain
    $run cargo doc
done
