#!/bin/bash

# This runs only when a commit is pushed to master. It is responsible for
# updating docs and computing coverage statistics.

set -e

if [ "$TRAVIS_RUST_VERSION" != "nightly" ] || [ "$TRAVIS_PULL_REQUEST" != "false" ] || [ "$TRAVIS_BRANCH" != "master" ]; then
  exit 0
fi

env

# Install kcov.
tmp=$(mktemp -d)
pushd "$tmp"
wget https://github.com/SimonKagstrom/kcov/archive/master.tar.gz
tar zxf master.tar.gz
mkdir kcov-master/build
cd kcov-master/build
cmake ..
make
make install DESTDIR="$tmp"
popd
PATH="$tmp/usr/local/bin:$PATH" ./ci/run-kcov --coveralls-id $TRAVIS_JOB_ID
