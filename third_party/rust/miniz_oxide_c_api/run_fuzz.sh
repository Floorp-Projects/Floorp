#!/usr/bin/env bash

OLD="crate-type = \['staticlib', 'rlib'\]"
NEW="crate-type = \['rlib'\]"

sed -i "s/$OLD/$NEW/g" Cargo.toml
cargo fuzz run fuzz_high -- -max_len=900
