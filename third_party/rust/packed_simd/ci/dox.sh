#!/bin/sh

set -ex

rm -rf target/doc
mkdir -p target/doc

# Build API documentation
cargo doc --features=into_bits

# Build Performance Guide
# FIXME: https://github.com/rust-lang-nursery/mdBook/issues/780
# mdbook build perf-guide -d target/doc/perf-guide
cd perf-guide
mdbook build
cd -
cp -r perf-guide/book target/doc/perf-guide

# If we're on travis, not a PR, and on the right branch, publish!
if [ "$TRAVIS_PULL_REQUEST" = "false" ] && [ "$TRAVIS_BRANCH" = "master" ]; then
  python3 -vV
  pip -vV
  python3.9 -vV
  pip install ghp_import --user
  ghp-import -n target/doc
  git push -qf https://${GH_PAGES}@github.com/${TRAVIS_REPO_SLUG}.git gh-pages
fi
