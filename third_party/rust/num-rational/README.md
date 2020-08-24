# num-rational

[![crate](https://img.shields.io/crates/v/num-rational.svg)](https://crates.io/crates/num-rational)
[![documentation](https://docs.rs/num-rational/badge.svg)](https://docs.rs/num-rational)
[![minimum rustc 1.31](https://img.shields.io/badge/rustc-1.31+-red.svg)](https://rust-lang.github.io/rfcs/2495-min-rust-version.html)
[![build status](https://github.com/rust-num/num-rational/workflows/master/badge.svg)](https://github.com/rust-num/num-rational/actions)

Generic `Rational` numbers (aka fractions) for Rust.

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
num-rational = "0.3"
```

## Features

This crate can be used without the standard library (`#![no_std]`) by disabling
the default `std` feature.  Use this in `Cargo.toml`:

```toml
[dependencies.num-rational]
version = "0.3"
default-features = false
```

## Releases

Release notes are available in [RELEASES.md](RELEASES.md).

## Compatibility

The `num-rational` crate is tested for rustc 1.31 and greater.
