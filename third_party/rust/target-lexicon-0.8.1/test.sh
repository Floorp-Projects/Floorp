#!/bin/bash
set -oeu pipefail


for trip in wasm32-unknown-unknown wasm32-wasi arm-unknown-linux-gnueabi aarch64-unknown-linux-gnu; do
     echo TARGET $trip
  cargo build --target $trip
  cp target/$trip/debug/build/target-lexicon-*/out/host.rs host.rs
  rustfmt host.rs
  diff -u target/$trip/debug/build/target-lexicon-*/out/host.rs host.rs
done
