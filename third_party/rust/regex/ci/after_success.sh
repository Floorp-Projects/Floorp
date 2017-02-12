#!/bin/bash

# This runs only when a commit is pushed to master. It is responsible for
# updating docs and computing coverage statistics.

set -e

if [ "$TRAVIS_RUST_VERSION" != "nightly" ] || [ "$TRAVIS_PULL_REQUEST" != "false" ] || [ "$TRAVIS_BRANCH" != "master" ]; then
  exit 0
fi

env

# Build and upload docs.
echo '<meta http-equiv=refresh content=0;url=regex/index.html>' > target/doc/index.html
ve=$(mktemp -d)
virtualenv "$ve"
"$ve"/bin/pip install --upgrade pip
"$ve"/bin/pip install ghp-import
"$ve"/bin/ghp-import -n target/doc
git push -qf https://${TOKEN}@github.com/${TRAVIS_REPO_SLUG}.git gh-pages

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
