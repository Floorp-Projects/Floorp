# flate2

[![Crates.io](https://img.shields.io/crates/v/flate2.svg?maxAge=2592000)](https://crates.io/crates/flate2)
[![Documentation](https://docs.rs/flate2/badge.svg)](https://docs.rs/flate2)

A streaming compression/decompression library DEFLATE-based streams in Rust.

This crate by default uses the `miniz_oxide` crate, a port of `miniz.c` to pure
Rust. This crate also supports other [backends](#Backends), such as the widely
available zlib library or the high-performance zlib-ng library.

Supported formats:

* deflate
* zlib
* gzip

```toml
# Cargo.toml
[dependencies]
flate2 = "1.0"
```

## Compression

```rust
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
use std::io::prelude::*;
use flate2::read::GzDecoder;

fn main() {
    let mut d = GzDecoder::new("...".as_bytes());
    let mut s = String::new();
    d.read_to_string(&mut s).unwrap();
    println!("{}", s);
}
```

## Backends

The default `miniz_oxide` backend has the advantage of being pure Rust, but if
you're already using zlib with another C library, for example, you can use that
for Rust code as well:

```toml
[dependencies]
flate2 = { version = "1.0.17", features = ["zlib"], default-features = false }
```

This supports either the high-performance zlib-ng backend (in zlib-compat mode)
or the use of a shared system zlib library. To explicitly opt into the fast
zlib-ng backend, use:

```toml
[dependencies]
flate2 = { version = "1.0.17", features = ["zlib-ng-compat"], default-features = false }
```

Note that if any crate in your dependency graph explicitly requests stock zlib,
or uses libz-sys directly without `default-features = false`, you'll get stock
zlib rather than zlib-ng. See [the libz-sys
README](https://github.com/rust-lang/libz-sys/blob/main/README.md) for details.

For compatibility with previous versions of `flate2`, the cloudflare optimized
version of zlib is available, via the `cloudflare_zlib` feature. It's not as
fast as zlib-ng, but it's faster than stock zlib. It requires a x86-64 CPU with
SSE 4.2 or ARM64 with NEON & CRC. It does not support 32-bit CPUs at all and is
incompatible with mingw. For more information check the [crate
documentation](https://crates.io/crates/cloudflare-zlib-sys). Note that
`cloudflare_zlib` will cause breakage if any other crate in your crate graph
uses another version of zlib/libz.

For compatibility with previous versions of `flate2`, the C version of `miniz.c`
is still available, using the feature `miniz-sys`.

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
