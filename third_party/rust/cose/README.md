# cose-rust

A Rust library for [COSE](https://tools.ietf.org/html/rfc8152) using [NSS](https://github.com/nss-dev/nss/).

[![Build Status](https://travis-ci.org/franziskuskiefer/cose-rust.svg?branch=master)](https://travis-ci.org/franziskuskiefer/cose-rust/)
![Maturity Level](https://img.shields.io/badge/maturity-alpha-red.svg)

**THIS IS WORK IN PROGRESS. DO NOT USE YET.**

## Build instructions

If NSS is not installed in the path, use `NSS_LIB_DIR` to set the library path where
we can find the NSS libraries.

    cargo build

### Run Tests and Examples

To run tests and examples you need NSS in your library path. Tests can be run
with

    cargo test

and examples with

    cargo run --example sign_verify
