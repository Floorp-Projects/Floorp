bitflags
========

A Rust macro to generate structures which behave like a set of bitflags

[![Build Status](https://travis-ci.org/rust-lang-nursery/bitflags.svg?branch=master)](https://travis-ci.org/rust-lang-nursery/bitflags)

[Documentation](https://doc.rust-lang.org/bitflags)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
bitflags = "0.7"
```

and this to your crate root:

```rust
#[macro_use]
extern crate bitflags;
```

## 128-bit integer bitflags (nightly only)

Add this to your `Cargo.toml`:

```toml
[dependencies.bitflags]
version = "0.7"
features = ["i128"]
```

and this to your crate root:

```rust
#![feature(i128_type)]

#[macro_use]
extern crate bitflags;
```
