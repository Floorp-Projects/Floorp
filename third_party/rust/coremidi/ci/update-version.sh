#!/bin/bash

set -ex

VERSION=${TRAVIS_TAG:-$(git describe --tags)}

sed -i '' "s/version = \".*\"/version = \"$VERSION\"/g" Cargo.toml
