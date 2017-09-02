#!/usr/bin/env bash

set -xeu
cd "$(dirname "$0")/.."

cargo check --features "$BINDGEN_FEATURES testing_only_docs"
