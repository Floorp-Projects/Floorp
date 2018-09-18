# num-traits

[![crate](https://img.shields.io/crates/v/num-traits.svg)](https://crates.io/crates/num-traits)
[![documentation](https://docs.rs/num-traits/badge.svg)](https://docs.rs/num-traits)
![minimum rustc 1.8](https://img.shields.io/badge/rustc-1.8+-red.svg)
[![Travis status](https://travis-ci.org/rust-num/num-traits.svg?branch=master)](https://travis-ci.org/rust-num/num-traits)

Numeric traits for generic mathematics in Rust.

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
num-traits = "0.2"
```

and this to your crate root:

```rust
extern crate num_traits;
```

## Features

This crate can be used without the standard library (`#![no_std]`) by disabling
the default `std` feature. Use this in `Cargo.toml`:

```toml
[dependencies.num-traits]
version = "0.2"
default-features = false
```

The `Float` and `Real` traits are only available when `std` is enabled. The
`FloatCore` trait is always available.  `MulAdd` and `MulAddAssign` for `f32`
and `f64` also require `std`, as do implementations of signed and floating-
point exponents in `Pow`.

Implementations for `i128` and `u128` are only available with Rust 1.26 and
later.  The build script automatically detects this, but you can make it
mandatory by enabling the `i128` crate feature.

## Releases

Release notes are available in [RELEASES.md](RELEASES.md).

## Compatibility

The `num-traits` crate is tested for rustc 1.8 and greater.
