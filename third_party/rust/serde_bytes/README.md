# serde\_bytes [![Build Status](https://api.travis-ci.org/serde-rs/bytes.svg?branch=master)](https://travis-ci.org/serde-rs/bytes) [![Latest Version](https://img.shields.io/crates/v/serde_bytes.svg)](https://crates.io/crates/serde_bytes)

Wrapper types to enable optimized handling of `&[u8]` and `Vec<u8>`.

Without specialization, Rust forces Serde to treat `&[u8]` just like any
other slice and `Vec<u8>` just like any other vector. In reality this
particular slice and vector can often be serialized and deserialized in a
more efficient, compact representation in many formats.

When working with such a format, you can opt into specialized handling of
`&[u8]` by wrapping it in `serde_bytes::Bytes` and `Vec<u8>` by wrapping it
in `serde_bytes::ByteBuf`.

This crate supports the Serde `with` attribute to enable efficient handling
of `&[u8]` and `Vec<u8>` in structs without needing a wrapper type.

```rust
#[macro_use]
extern crate serde_derive;

extern crate serde;
extern crate serde_bytes;

#[derive(Serialize)]
struct Efficient<'a> {
    #[serde(with = "serde_bytes")]
    bytes: &'a [u8],

    #[serde(with = "serde_bytes")]
    byte_buf: Vec<u8>,
}

#[derive(Serialize, Deserialize)]
struct Packet {
    #[serde(with = "serde_bytes")]
    payload: Vec<u8>,
}
```

## License

Serde is licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or
   http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or
   http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in Serde by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
