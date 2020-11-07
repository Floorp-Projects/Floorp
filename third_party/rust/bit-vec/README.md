A Vec of bits.

Documentation is available at https://contain-rs.github.io/bit-vec/bit_vec.

[![Build Status](https://travis-ci.org/contain-rs/bit-vec.svg?branch=master)](https://travis-ci.org/contain-rs/bit-vec)
[![crates.io](http://meritbadge.herokuapp.com/bit-vec)](https://crates.io/crates/bit-vec)

## Usage

Add this to your Cargo.toml:

```toml
[dependencies]
bit-vec = "0.6"
```

and this to your crate root:

```rust
extern crate bit_vec;
```

If you want [serde](https://github.com/serde-rs/serde) support, include the feature like this:

```toml
[dependencies]
bit-vec = { version = "0.6", features = ["serde"] }
```

If you want to use bit-vec in a program that has `#![no_std]`, just drop default features:

```toml
[dependencies]
bit-vec = { version = "0.6", default-features = false }
```
