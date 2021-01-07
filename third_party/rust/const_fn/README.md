# \#\[const_fn\]

[![crates.io](https://img.shields.io/crates/v/const_fn.svg?style=flat-square&logo=rust)](https://crates.io/crates/const_fn)
[![docs.rs](https://img.shields.io/badge/docs.rs-const__fn-blue?style=flat-square)](https://docs.rs/const_fn)
[![license](https://img.shields.io/badge/license-Apache--2.0_OR_MIT-blue.svg?style=flat-square)](#license)
[![rustc](https://img.shields.io/badge/rustc-1.31+-blue.svg?style=flat-square)](https://www.rust-lang.org)
[![build status](https://img.shields.io/github/workflow/status/taiki-e/const_fn/CI/master?style=flat-square)](https://github.com/taiki-e/const_fn/actions?query=workflow%3ACI+branch%3Amaster)

An attribute for easy generation of const functions with conditional
compilations.

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
const_fn = "0.4"
```

*Compiler support: requires rustc 1.31+*

## Examples

```rust
use const_fn::const_fn;

// function is `const` on specified version and later compiler (including beta and nightly)
#[const_fn("1.36")]
pub const fn version() {
    /* ... */
}

// function is `const` on nightly compiler (including dev build)
#[const_fn(nightly)]
pub const fn nightly() {
    /* ... */
}

// function is `const` if `cfg(...)` is true
#[const_fn(cfg(...))]
pub const fn cfg() {
    /* ... */
}

// function is `const` if `cfg(feature = "...")` is true
#[const_fn(feature = "...")]
pub const fn feature() {
    /* ... */
}
```

## Alternatives

This crate is proc-macro, but is very lightweight, and has no dependencies.

You can manually define declarative macros with similar functionality (see
[`if_rust_version`](https://github.com/ogoffart/if_rust_version#examples)),
or [you can define the same function twice with different cfg](https://github.com/crossbeam-rs/crossbeam/blob/0b6ea5f69fde8768c1cfac0d3601e0b4325d7997/crossbeam-epoch/src/atomic.rs#L340-L372).
(Note: the former approach requires more macros to be defined depending on the
number of version requirements, the latter approach requires more functions to
be maintained manually)

## License

Licensed under either of [Apache License, Version 2.0](LICENSE-APACHE) or
[MIT license](LICENSE-MIT) at your option.

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
