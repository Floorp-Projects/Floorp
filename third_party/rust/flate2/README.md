# flate2

[![Build Status](https://dev.azure.com/alexcrichton/flate2-rs/_apis/build/status/alexcrichton.flate2-rs?branchName=master)](https://dev.azure.com/alexcrichton/flate2-rs/_build/latest?definitionId=1&branchName=master)
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
flate2 = "1.0"
```

Using zlib instead of miniz:

```toml
[dependencies]
flate2 = { version = "1.0", features = ["zlib"], default-features = false }
```

Using the rust back-end:

```toml
[dependencies]
flate2 = { version = "1.0", features = ["rust_backend"], default-features = false }
```

## Compression

```rust
extern crate flate2;

use std::io::prelude::*;
use flate2::Compression;
use flate2::write::ZlibEncoder;

fn main() {
    let mut e = ZlibEncoder::new(Vec::new(), Compression::default());
    e.write_all(b"foo");
    e.write_all(b"bar");
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
