# num

A collection of numeric types and traits for Rust.

This includes new types for big integers, rationals, and complex numbers,
new traits for generic programming on numeric properties like `Integer,
and generic range iterators.

[Documentation](http://rust-num.github.io/num)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
num = "0.1"
```

and this to your crate root:

```rust
extern crate num;
```

## Compatibility

Most of the `num` crates are tested for rustc 1.8 and greater.
The exceptions are `num-derive` which requires at least rustc 1.15,
and the deprecated `num-macros` which requires nightly rustc.
