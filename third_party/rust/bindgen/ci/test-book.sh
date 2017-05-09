#!/usr/bin/env bash

set -xeu
cd "$(dirname "$0")/../book"

cargo install mdbook || true
export PATH="$PATH:~/.cargo/bin"

mdbook build
mdbook test
