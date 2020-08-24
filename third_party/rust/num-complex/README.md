# num-complex

[![crate](https://img.shields.io/crates/v/num-complex.svg)](https://crates.io/crates/num-complex)
[![documentation](https://docs.rs/num-complex/badge.svg)](https://docs.rs/num-complex)
[![minimum rustc 1.31](https://img.shields.io/badge/rustc-1.31+-red.svg)](https://rust-lang.github.io/rfcs/2495-min-rust-version.html)
[![build status](https://github.com/rust-num/num-complex/workflows/master/badge.svg)](https://github.com/rust-num/num-complex/actions)

`Complex` numbers for Rust.

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
num-complex = "0.3"
```

## Features

This crate can be used without the standard library (`#![no_std]`) by disabling
the default `std` feature. Use this in `Cargo.toml`:

```toml
[dependencies.num-complex]
version = "0.3"
default-features = false
```

Features based on `Float` types are only available when `std` or `libm` is
enabled. Where possible, `FloatCore` is used instead.  Formatting complex
numbers only supports format width when `std` is enabled.

## Releases

Release notes are available in [RELEASES.md](RELEASES.md).

## Compatibility

The `num-complex` crate is tested for rustc 1.31 and greater.
