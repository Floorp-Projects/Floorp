Serde repr derive
=================

[![Build Status](https://api.travis-ci.com/dtolnay/serde-repr.svg?branch=master)](https://travis-ci.com/dtolnay/serde-repr)
[![Latest Version](https://img.shields.io/crates/v/serde_repr.svg)](https://crates.io/crates/serde_repr)
[![Rust Documentation](https://img.shields.io/badge/api-rustdoc-blue.svg)](https://docs.rs/serde_repr)

This crate provides a derive macro to derive Serde's `Serialize` and
`Deserialize` traits in a way that delegates to the underlying repr of a C-like
enum.

```toml
[dependencies]
serde = "1.0"
serde_repr = "0.1"
```

```rust
use serde_repr::{Serialize_repr, Deserialize_repr};

#[derive(Serialize_repr, Deserialize_repr, PartialEq, Debug)]
#[repr(u8)]
enum SmallPrime {
    Two = 2,
    Three = 3,
    Five = 5,
    Seven = 7,
}

fn main() -> serde_json::Result<()> {
    let j = serde_json::to_string(&SmallPrime::Seven)?;
    assert_eq!(j, "7");

    let p: SmallPrime = serde_json::from_str("2")?;
    assert_eq!(p, SmallPrime::Two);

    Ok(())
}
```

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
