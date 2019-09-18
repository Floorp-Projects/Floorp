#!/bin/sh
# Use rustup to locally run the same suite of tests as .travis.yml.
# (You should first install/update 1.8.0, stable, beta, and nightly.)

set -ex

export TRAVIS_RUST_VERSION
for TRAVIS_RUST_VERSION in 1.8.0 stable beta nightly; do
    run="rustup run $TRAVIS_RUST_VERSION"
    if [ "$TRAVIS_RUST_VERSION" = 1.8.0 ]; then
      # rand 0.3.22 started depending on rand 0.4, which requires rustc 1.15
      # manually hacking the lockfile due to the limitations of cargo#2773
      $run cargo generate-lockfile
      $run sed -i -e 's/"rand 0.[34].[0-9]\+/"rand 0.3.20/' Cargo.lock
      $run sed -i -e '/^name = "rand"/,/^$/s/version = "0.3.[0-9]\+"/version = "0.3.20"/' Cargo.lock
    fi
    $run cargo build --verbose
    $run $PWD/ci/test_full.sh
done
