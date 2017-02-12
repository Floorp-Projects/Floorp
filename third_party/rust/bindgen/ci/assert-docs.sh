#!/usr/bin/env bash

set -xeu
cd "$(dirname "$0")/.."

cargo build --features "$BINDGEN_FEATURES docs_"
