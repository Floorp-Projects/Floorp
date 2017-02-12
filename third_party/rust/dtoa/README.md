dtoa
====

[![Build Status](https://api.travis-ci.org/dtolnay/dtoa.svg?branch=master)](https://travis-ci.org/dtolnay/dtoa)
[![Latest Version](https://img.shields.io/crates/v/dtoa.svg)](https://crates.io/crates/dtoa)

This crate provides fast functions for printing floating-point primitives to an
[`io::Write`](https://doc.rust-lang.org/std/io/trait.Write.html). The
implementation is a straightforward Rust port of [Milo
Yip](https://github.com/miloyip)'s C++ implementation
[dtoa.h](https://github.com/miloyip/rapidjson/blob/master/include/rapidjson/internal/dtoa.h).
The original C++ code of each function is included in comments.

See also [`itoa`](https://github.com/dtolnay/itoa) for printing integer
primitives.

## Performance

![performance](https://raw.githubusercontent.com/dtolnay/dtoa/master/performance.png)

## Functions

```rust
extern crate dtoa;

// write to a vector or other io::Write
let mut buf = Vec::new();
dtoa::write(&mut buf, 2.71828f64)?;
println!("{:?}", buf);

// write to a stack buffer
let mut bytes = [b'\0'; 20];
let n = dtoa::write(&mut bytes[..], 2.71828f64)?;
println!("{:?}", &bytes[..n]);
```

The function signature is:

```rust
fn write<W: io::Write, V: dtoa::Floating>(writer: W, value: V) -> io::Result<()>
```

where `dtoa::Floating` is implemented for `f32` and `f64`. The return value
gives the number of bytes written.

## Dependency

Dtoa is available on [crates.io](https://crates.io/crates/dtoa). Use the
following in `Cargo.toml`:

```toml
[dependencies]
dtoa = "0.4"
```

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in dtoa by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
