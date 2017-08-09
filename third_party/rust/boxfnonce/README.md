[![Travis Build Status](https://travis-ci.org/stbuehler/rust-boxfnonce.svg?branch=master)](https://travis-ci.org/stbuehler/rust-boxfnonce)
[![crates.io](https://img.shields.io/crates/v/boxfnonce.svg)](https://crates.io/crates/boxfnonce)
[![MIT licensed](https://img.shields.io/badge/license-MIT-blue.svg)](./LICENSE)

This library provide a safe way to box FnOnce types.  It doesn't use any
unstable features and is therefore fully compatible with rust stable.

This library is provided because `Box<FnOnce()>` doesn't work yet, and
`Box<FnBox()>` will never be available in rust stable.

The documentation is located at [https://stbuehler.github.io/rustdocs/boxfnonce/boxfnonce/](https://stbuehler.github.io/rustdocs/boxfnonce/boxfnonce/).
