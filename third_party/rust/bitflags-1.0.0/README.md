bitflags
========

A Rust macro to generate structures which behave like a set of bitflags

[![Build Status](https://travis-ci.org/rust-lang-nursery/bitflags.svg?branch=master)](https://travis-ci.org/rust-lang-nursery/bitflags)

- [Documentation](https://docs.rs/bitflags)
- [Release notes](https://github.com/rust-lang-nursery/bitflags/releases)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
bitflags = "1.0"
```

and this to your crate root:

```rust
#[macro_use]
extern crate bitflags;
```

## Rust Version Support

The minimum supported* Rust version is 1.20 due to use of associated constants.

_* As of the current master branch (unreleased)_
