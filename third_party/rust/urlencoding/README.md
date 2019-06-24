# urlencoding

[![Latest Version](https://img.shields.io/crates/v/urlencoding.svg)](https://crates.io/crates/urlencoding)

A Rust library for doing URL percentage encoding.

Installation
============

This crate can be downloaded through Cargo. To do so, add the following line to your `Cargo.toml` file, under `dependencies`:

```toml
urlencoding = "1.0.0"
```

Usage
=====

To encode a string, do the following:

```rust
extern crate urlencoding;

use urlencoding::encode;

fn main() {
  let encoded = encode("This string will be URL encoded.");
  println!("{}", encoded);
  // This%20string%20will%20be%20URL%20encoded.
}
```

To decode a string, it's only slightly different:

```rust
extern crate urlencoding;

use urlencoding::decode;

fn main() {
  let decoded = decode("%F0%9F%91%BE%20Exterminate%21");
  println!("{}", decoded.unwrap());
  // 👾 Exterminate!
}
```

License
=======

This project is licensed under the MIT license, Copyright (c) 2017 Bertram Truong. For more information see the `LICENSE` file.
