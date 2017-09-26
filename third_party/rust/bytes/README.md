# Bytes

A utility library for working with bytes.

[![Crates.io](https://img.shields.io/crates/v/bytes.svg?maxAge=2592000)](https://crates.io/crates/bytes)
[![Build Status](https://travis-ci.org/carllerche/bytes.svg?branch=master)](https://travis-ci.org/carllerche/bytes)

[Documentation](https://carllerche.github.io/bytes/bytes/index.html)

## Usage

To use `bytes`, first add this to your `Cargo.toml`:

```toml
[dependencies]
bytes = "0.4"
```

Next, add this to your crate:

```rust
extern crate bytes;

use bytes::{Bytes, BytesMut, Buf, BufMut};
```

## Serde support

Serde support is optional and disabled by default. To enable use the feature `serde`.

```toml
[dependencies]
bytes = { version = "0.4", features = ["serde"] }
```

# License

`bytes` is primarily distributed under the terms of both the MIT license and the
Apache License (Version 2.0), with portions covered by various BSD-like
licenses.

See LICENSE-APACHE, and LICENSE-MIT for details.
