lazy-static.rs
==============

[![Travis-CI Status](https://travis-ci.org/rust-lang-nursery/lazy-static.rs.svg?branch=master)](https://travis-ci.org/rust-lang-nursery/lazy-static.rs)

A macro for declaring lazily evaluated statics in Rust.

Using this macro, it is possible to have `static`s that require code to be
executed at runtime in order to be initialized.
This includes anything requiring heap allocations, like vectors or hash maps,
as well as anything that requires function calls to be computed.

# Syntax

```rust
lazy_static! {
    [pub] static ref NAME_1: TYPE_1 = EXPR_1;
    [pub] static ref NAME_2: TYPE_2 = EXPR_2;
    ...
    [pub] static ref NAME_N: TYPE_N = EXPR_N;
}
```

# Semantic

For a given `static ref NAME: TYPE = EXPR;`, the macro generates a
unique type that implements `Deref<TYPE>` and stores it in a static with name `NAME`.

On first deref, `EXPR` gets evaluated and stored internally, such that all further derefs
can return a reference to the same object.

Like regular `static mut`s, this macro only works for types that fulfill the `Sync`
trait.

# Getting Started

[lazy-static.rs is available on crates.io](https://crates.io/crates/lazy_static).
Add the following dependency to your Cargo manifest to get the latest version of the 0.1 branch:

```toml
[dependencies]
lazy_static = "0.1.*"
```

To always get the latest version, add this git repository to your
Cargo manifest:

```toml
[dependencies.lazy_static]
git = "https://github.com/rust-lang-nursery/lazy-static.rs"
```
# Example

Using the macro:

```rust
#[macro_use]
extern crate lazy_static;

use std::collections::HashMap;

lazy_static! {
    static ref HASHMAP: HashMap<u32, &'static str> = {
        let mut m = HashMap::new();
        m.insert(0, "foo");
        m.insert(1, "bar");
        m.insert(2, "baz");
        m
    };
    static ref COUNT: usize = HASHMAP.len();
    static ref NUMBER: u32 = times_two(21);
}

fn times_two(n: u32) -> u32 { n * 2 }

fn main() {
    println!("The map has {} entries.", *COUNT);
    println!("The entry for `0` is \"{}\".", HASHMAP.get(&0).unwrap());
    println!("An expensive calculation on a static results in: {}.", *NUMBER);
}
```
