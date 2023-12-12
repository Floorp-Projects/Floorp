#!/usr/bin/env bash

set -eu

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

for example_path in examples/*.rs; do
    example_filename=$(basename -- $example_path)
    example=${example_filename%.*}
    echo $example
    cargo valgrind run --example "$example"
done
