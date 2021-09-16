<div align="center">

  <h1><code>Arbitrary</code></h1>

  <p><strong>The trait for generating structured data from arbitrary, unstructured input.</strong></p>

  <img alt="GitHub Actions Status" src="https://github.com/rust-fuzz/rust_arbitrary/workflows/Rust/badge.svg"/>

</div>

## About

The `Arbitrary` crate lets you construct arbitrary instances of a type.

This crate is primarily intended to be combined with a fuzzer like [libFuzzer
and `cargo-fuzz`](https://github.com/rust-fuzz/cargo-fuzz) or
[AFL](https://github.com/rust-fuzz/afl.rs), and to help you turn the raw,
untyped byte buffers that they produce into well-typed, valid, structured
values. This allows you to combine structure-aware test case generation with
coverage-guided, mutation-based fuzzers.

## Documentation

[**Read the API documentation on `docs.rs`!**](https://docs.rs/arbitrary)

## Example

Say you're writing a color conversion library, and you have an `Rgb` struct to
represent RGB colors. You might want to implement `Arbitrary` for `Rgb` so that
you could take arbitrary `Rgb` instances in a test function that asserts some
property (for example, asserting that RGB converted to HSL and converted back to
RGB always ends up exactly where we started).

### Automatically Deriving `Arbitrary`

Automatically deriving the `Arbitrary` trait is the recommended way to implement
`Arbitrary` for your types.

Automatically deriving `Arbitrary` requires you to enable the `"derive"` cargo
feature:

```toml
# Cargo.toml

[dependencies]
arbitrary = { version = "1", features = ["derive"] }
```

And then you can simply add `#[derive(Arbitrary)]` annotations to your types:

```rust
// rgb.rs

use arbitrary::Arbitrary;

#[derive(Arbitrary)]
pub struct Rgb {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}
```

### Implementing `Arbitrary` By Hand

Alternatively, you can write an `Arbitrary` implementation by hand:

```rust
// rgb.rs

use arbitrary::{Arbitrary, Result, Unstructured};

#[derive(Copy, Clone, Debug)]
pub struct Rgb {
    pub r: u8,
    pub g: u8,
    pub b: u8,
}

impl<'a> Arbitrary<'a> for Rgb {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self> {
        let r = u8::arbitrary(u)?;
        let g = u8::arbitrary(u)?;
        let b = u8::arbitrary(u)?;
        Ok(Rgb { r, g, b })
    }
}
```

## License

Licensed under dual MIT or Apache-2.0 at your choice.

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this project by you, as defined in the Apache-2.0 license,
shall be dual licensed as above, without any additional terms or conditions.
