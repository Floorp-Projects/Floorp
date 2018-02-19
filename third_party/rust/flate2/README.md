# flate2

[![Build Status](https://travis-ci.org/alexcrichton/flate2-rs.svg?branch=master)](https://travis-ci.org/alexcrichton/flate2-rs)
[![Build status](https://ci.appveyor.com/api/projects/status/9tatexq47i3ee13k?svg=true)](https://ci.appveyor.com/project/alexcrichton/flate2-rs)
[![Crates.io](https://img.shields.io/crates/v/flate2.svg?maxAge=2592000)](https://crates.io/crates/flate2)
[![Documentation](https://docs.rs/flate2/badge.svg)](https://docs.rs/flate2)

A streaming compression/decompression library for Rust. The underlying
implementation by default uses [`miniz`](https://github.com/richgel999/miniz) but
can optionally be configured to use the system zlib, if available.

There is also an experimental rust backend that uses the
[`miniz_oxide`](https://crates.io/crates/miniz_oxide) crate. This avoids the need
to build C code, but hasn't gone through as much testing as the other backends.

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

Using the rust back-end:

```toml
[dependencies]
flate2 = { version = "0.2", features = ["rust_backend"], default-features = false }
```

## Compression

```rust
extern crate flate2;

use std::io::prelude::*;
use flate2::Compression;
use flate2::write::ZlibEncoder;

fn main() {
    let mut e = ZlibEncoder::new(Vec::new(), Compression::default());
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
    let mut d = GzDecoder::new("...".as_bytes());
    let mut s = String::new();
    d.read_to_string(&mut s).unwrap();
    println!("{}", s);
}
```

# License

This project is licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or
   http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or
   http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this project by you, as defined in the Apache-2.0 license,
shall be dual licensed as above, without any additional terms or conditions.
