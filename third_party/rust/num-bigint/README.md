# num-bigint

[![crate](https://img.shields.io/crates/v/num-bigint.svg)](https://crates.io/crates/num-bigint)
[![documentation](https://docs.rs/num-bigint/badge.svg)](https://docs.rs/num-bigint)
![minimum rustc 1.8](https://img.shields.io/badge/rustc-1.8+-red.svg)
[![Travis status](https://travis-ci.org/rust-num/num-bigint.svg?branch=master)](https://travis-ci.org/rust-num/num-bigint)

Big integer types for Rust, `BigInt` and `BigUint`.

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
num-bigint = "0.1"
```

and this to your crate root:

```rust
extern crate num_bigint;
```

## Releases

Release notes are available in [RELEASES.md](RELEASES.md).

## Compatibility

The `num-bigint` crate is tested for rustc 1.8 and greater.

## Alternatives

While `num-bigint` strives for good performance in pure Rust code, other
crates may offer better performance with different trade-offs.  The following
table offers a brief comparison to a few alternatives.

| Crate            | License        | Min rustc | Implementation |
| :--------------- | :------------- | :-------- | :------------- |
| **`num-bigint`** | MIT/Apache-2.0 | 1.8       | pure rust |
| [`ramp`]         | Apache-2.0     | nightly   | rust and inline assembly |
| [`rug`]          | LGPL-3.0+      | 1.18      | bundles [GMP] via [`gmp-mpfr-sys`] |
| [`rust-gmp`]     | MIT            | stable?   | links to [GMP] |
| [`apint`]        | MIT/Apache-2.0 | nightly   | pure rust (unfinished) |

[GMP]: https://gmplib.org/
[`gmp-mpfr-sys`]: https://crates.io/crates/gmp-mpfr-sys
[`rug`]: https://crates.io/crates/rug
[`rust-gmp`]: https://crates.io/crates/rust-gmp
[`ramp`]: https://crates.io/crates/ramp
[`apint`]: https://crates.io/crates/apint
