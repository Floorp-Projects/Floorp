itoa
====

[![Build Status](https://api.travis-ci.org/dtolnay/itoa.svg?branch=master)](https://travis-ci.org/dtolnay/itoa)
[![Latest Version](https://img.shields.io/crates/v/itoa.svg)](https://crates.io/crates/itoa)
[![Rust Documentation](https://img.shields.io/badge/api-rustdoc-blue.svg)](https://docs.rs/itoa)

This crate provides fast functions for printing integer primitives to an
[`io::Write`] or a [`fmt::Write`]. The implementation comes straight from
[libcore] but avoids the performance penalty of going through
[`fmt::Formatter`].

See also [`dtoa`] for printing floating point primitives.

*Version requirement: rustc 1.0+*

[`io::Write`]: https://doc.rust-lang.org/std/io/trait.Write.html
[`fmt::Write`]: https://doc.rust-lang.org/core/fmt/trait.Write.html
[libcore]: https://github.com/rust-lang/rust/blob/b8214dc6c6fc20d0a660fb5700dca9ebf51ebe89/src/libcore/fmt/num.rs#L201-L254
[`fmt::Formatter`]: https://doc.rust-lang.org/std/fmt/struct.Formatter.html
[`dtoa`]: https://github.com/dtolnay/dtoa

```toml
[dependencies]
itoa = "0.4"
```

<br>

## Performance (lower is better)

![performance](https://raw.githubusercontent.com/dtolnay/itoa/master/performance.png)

<br>

## Examples

```rust
use std::{fmt, io};

fn demo_itoa_write() -> io::Result<()> {
    // Write to a vector or other io::Write.
    let mut buf = Vec::new();
    itoa::write(&mut buf, 128u64)?;
    println!("{:?}", buf);

    // Write to a stack buffer.
    let mut bytes = [0u8; 20];
    let n = itoa::write(&mut bytes[..], 128u64)?;
    println!("{:?}", &bytes[..n]);

    Ok(())
}

fn demo_itoa_fmt() -> fmt::Result {
    // Write to a string.
    let mut s = String::new();
    itoa::fmt(&mut s, 128u64)?;
    println!("{}", s);

    Ok(())
}
```

The function signatures are:

```rust
fn write<W: io::Write, V: itoa::Integer>(writer: W, value: V) -> io::Result<usize>;

fn fmt<W: fmt::Write, V: itoa::Integer>(writer: W, value: V) -> fmt::Result;
```

where `itoa::Integer` is implemented for i8, u8, i16, u16, i32, u32, i64, u64,
i128, u128, isize and usize. 128-bit integer support requires rustc 1.26+ and
the `i128` feature of this crate enabled.

The `write` function is only available when the `std` feature is enabled
(default is enabled). The return value gives the number of bytes written.

<br>

#### License

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>
