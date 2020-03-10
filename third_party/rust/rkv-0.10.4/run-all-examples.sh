#!/bin/bash

set -e

cargo build --examples

for file in examples/*; do
    filename=$(basename ${file})
    extension=${filename##*.}
    example_name=${filename%.*}
    if [[ "${extension}" = "rs" ]]; then
        cargo run --example ${example_name}
    fi
done
