# flate2

[![Build Status](https://travis-ci.org/alexcrichton/flate2-rs.svg?branch=master)](https://travis-ci.org/alexcrichton/flate2-rs)
[![Build status](https://ci.appveyor.com/api/projects/status/9tatexq47i3ee13k?svg=true)](https://ci.appveyor.com/project/alexcrichton/flate2-rs)

[Documentation](https://docs.rs/flate2)

A streaming compression/decompression library for Rust. The underlying
implementation by default uses [`miniz`](https://code.google.com/p/miniz/) but
can optionally be configured to use the system zlib, if available.

Supported formats:

* deflate
* zlib
* gzip

```toml
# Cargo.toml
[dependencies]
flate2 = "0.2"
```

Using zlib instead of miniz:

```toml
[dependencies]
flate2 = { version = "0.2", features = ["zlib"], default-features = false }
```

## Compression

```rust
extern crate flate2;

use std::io::prelude::*;
use flate2::Compression;
use flate2::write::ZlibEncoder;

fn main() {
    let mut e = ZlibEncoder::new(Vec::new(), Compression::Default);
    e.write(b"foo");
    e.write(b"bar");
    let compressed_bytes = e.finish();
}
```

## Decompression

```rust,no_run
extern crate flate2;

use std::io::prelude::*;
use flate2::read::GzDecoder;

fn main() {
    let mut d = GzDecoder::new("...".as_bytes()).unwrap();
    let mut s = String::new();
    d.read_to_string(&mut s).unwrap();
    println!("{}", s);
}
```

# License

`flate2-rs` is primarily distributed under the terms of both the MIT license and
the Apache License (Version 2.0), with portions covered by various BSD-like
licenses.

See LICENSE-APACHE, and LICENSE-MIT for details.
