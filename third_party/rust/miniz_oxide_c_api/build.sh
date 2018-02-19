#!/usr/bin/env bash

cd $(dirname $0)

OLD="crate-type = \['rlib'\]"
NEW="crate-type = \['staticlib', 'rlib'\]"

sed -i "s/$OLD/$NEW/g" Cargo.toml

if [[ ($# == 0 || $1 == "--release" ) ]]; then
#    cargo rustc --release -- --emit asm
    cargo build --release --features=miniz_zip || exit 1
    cp target/release/libminiz_oxide_c_api.a .
elif [[ $1 == "--debug" ]]; then
    cargo build --features=miniz_zip || exit 1
    cp target/debug/libminiz_oxide_c_api.a .
else
    echo --relese or --debug
fi
