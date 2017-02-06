bitflags
========

A Rust macro to generate structures which behave like a set of bitflags

[![Build Status](https://travis-ci.org/rust-lang-nursery/bitflags.svg?branch=master)](https://travis-ci.org/rust-lang-nursery/bitflags)

[Documentation](https://doc.rust-lang.org/bitflags)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
bitflags = "0.6"
```

and this to your crate root:

```rust
#[macro_use]
extern crate bitflags;
```
