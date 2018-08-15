# num-derive

[![crate](https://img.shields.io/crates/v/num-derive.svg)](https://crates.io/crates/num-derive)
[![documentation](https://docs.rs/num-derive/badge.svg)](https://docs.rs/num-derive)
[![Travis status](https://travis-ci.org/rust-num/num-derive.svg?branch=master)](https://travis-ci.org/rust-num/num-derive)

Procedural macros to derive numeric traits in Rust.

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
num-traits = "0.2"
num-derive = "0.2"
```

and this to your crate root:

```rust
#[macro_use]
extern crate num_derive;
```

Then you can derive traits on your own types:

```rust
#[derive(FromPrimitive, ToPrimitive)]
enum Color {
    Red,
    Blue,
    Green,
}
```

## Optional features

- **`full-syntax`** — Enables `num-derive` to handle enum discriminants
  represented by complex expressions. Usually can be avoided by
  [utilizing constants], so only use this feature if namespace pollution is
  undesired and [compile time doubling] is acceptable.

[utilizing constants]: https://github.com/rust-num/num-derive/pull/3#issuecomment-359044704
[compile time doubling]: https://github.com/rust-num/num-derive/pull/3#issuecomment-359172588

## Releases

Release notes are available in [RELEASES.md](RELEASES.md).

## Compatibility

The `num-derive` crate is tested for rustc 1.15 and greater.
