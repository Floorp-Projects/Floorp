#!/usr/bin/env bash

cargo install cargo-benchcmp
cargo bench --features=benching > benchout
cargo benchcmp miniz:: oxide:: benchout
