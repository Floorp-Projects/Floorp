#!/bin/bash

set -ex

cargo login $CRATES_TOKEN

# --allow-dirty is required because Cargo.toml version has been updated by an script
cargo publish --allow-dirty
