# getrandom

[![Build Status](https://travis-ci.org/rust-random/getrandom.svg?branch=master)](https://travis-ci.org/rust-random/getrandom)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/rust-random/getrandom?svg=true)](https://ci.appveyor.com/project/rust-random/getrandom)
[![Crate](https://img.shields.io/crates/v/getrandom.svg)](https://crates.io/crates/getrandom)
[![Documentation](https://docs.rs/getrandom/badge.svg)](https://docs.rs/getrandom)
[![Dependency status](https://deps.rs/repo/github/rust-random/getrandom/status.svg)](https://deps.rs/repo/github/rust-random/getrandom)


A Rust library for retrieving random data from (operating) system source. It is
assumed that system always provides high-quality cryptographically secure random
data, ideally backed by hardware entropy sources. This crate derives its name
from Linux's `getrandom` function, but is cross platform, roughly supporting
the same set of platforms as Rust's `std` lib.

This is a low-level API. Most users should prefer using high-level random-number
library like [`rand`].

[`rand`]: https://crates.io/crates/rand


## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
getrandom = "0.1"
```

Then invoke the `getrandom` function:

```rust
fn get_random_buf() -> Result<[u8; 32], getrandom::Error> {
    let mut buf = [0u8; 32];
    getrandom::getrandom(&mut buf)?;
    buf
}
```

## Features

This library is `no_std` compatible, but uses `std` on most platforms.

The `log` library is supported as an optional dependency. If enabled, error
reporting will be improved on some platforms.

For WebAssembly (`wasm32`), WASI and Emscripten targets are supported directly; otherwise
one of the following features must be enabled:

-   [`wasm-bindgen`](https://crates.io/crates/wasm_bindgen)
-   [`stdweb`](https://crates.io/crates/stdweb)

## Minimum Supported Rust Version

This crate requires Rust 1.32.0 or later.


# License

The `getrandom` library is distributed under either of

 * [Apache License, Version 2.0](LICENSE-APACHE)
 * [MIT license](LICENSE-MIT)

at your option.

