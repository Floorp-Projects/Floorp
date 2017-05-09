#!/usr/bin/env bash

set -xeu
cd "$(dirname "$0")/../book"

# Ensure mdbook is installed.
cargo install mdbook || true
export PATH="$PATH:~/.cargo/bin"

# Get the git revision we are on.
rev=$(git rev-parse --short HEAD)

# Build the users guide book and go into the built book's directory.
rm -rf ./book
mdbook build
cd ./book

# Make the built book directory a new git repo, fetch upstream, make a new
# commit on gh-pages, and push it upstream.

git init
git config user.name "Travis CI"
git config user.email "builds@travis-ci.org"

git remote add upstream "https://$GH_TOKEN@github.com/servo/rust-bindgen.git"
git fetch upstream
git reset upstream/gh-pages

touch .

git add -A .
git commit -m "Rebuild users guide at ${rev}"
git push upstream HEAD:gh-pages
