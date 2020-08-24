# num-iter

[![crate](https://img.shields.io/crates/v/num-iter.svg)](https://crates.io/crates/num-iter)
[![documentation](https://docs.rs/num-iter/badge.svg)](https://docs.rs/num-iter)
![minimum rustc 1.8](https://img.shields.io/badge/rustc-1.8+-red.svg)
[![Travis status](https://travis-ci.org/rust-num/num-iter.svg?branch=master)](https://travis-ci.org/rust-num/num-iter)

Generic `Range` iterators for Rust.

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
num-iter = "0.1"
```

and this to your crate root:

```rust
extern crate num_iter;
```

## Features

This crate can be used without the standard library (`#![no_std]`) by disabling
the default `std` feature.  Use this in `Cargo.toml`:

```toml
[dependencies.num-iter]
version = "0.1.35"
default-features = false
```

There is no functional difference with and without `std` at this time, but
there may be in the future.

Implementations for `i128` and `u128` are only available with Rust 1.26 and
later.  The build script automatically detects this, but you can make it
mandatory by enabling the `i128` crate feature.



## Releases

Release notes are available in [RELEASES.md](RELEASES.md).

## Compatibility

The `num-iter` crate is tested for rustc 1.8 and greater.
