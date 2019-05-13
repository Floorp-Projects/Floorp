autocfg
=======

[![autocfg crate](https://img.shields.io/crates/v/autocfg.svg)](https://crates.io/crates/autocfg)
[![autocfg documentation](https://docs.rs/autocfg/badge.svg)](https://docs.rs/autocfg)
![minimum rustc 1.0](https://img.shields.io/badge/rustc-1.0+-red.svg)
[![Travis Status](https://travis-ci.org/cuviper/autocfg.svg?branch=master)](https://travis-ci.org/cuviper/autocfg)

A Rust library for build scripts to automatically configure code based on
compiler support.  Code snippets are dynamically tested to see if the `rustc`
will accept them, rather than hard-coding specific version support.


## Usage

Add this to your `Cargo.toml`:

```toml
[build-dependencies]
autocfg = "0.1"
```

Then use it in your `build.rs` script to detect compiler features.  For
example, to test for 128-bit integer support, it might look like:

```rust
extern crate autocfg;

fn main() {
    let ac = autocfg::new();
    ac.emit_has_type("i128");

    // (optional) We don't need to rerun for anything external.
    autocfg::rerun_path(file!());
}
```

If the type test succeeds, this will write a `cargo:rustc-cfg=has_i128` line
for Cargo, which translates to Rust arguments `--cfg has_i128`.  Then in the
rest of your Rust code, you can add `#[cfg(has_i128)]` conditions on code that
should only be used when the compiler supports it.


## Release Notes

- 0.1.2 (2018-01-16)
  - Add `rerun_env(ENV)` to print `cargo:rerun-if-env-changed=ENV`
  - Add `rerun_path(PATH)` to print `cargo:rerun-if-changed=PATH`


## Minimum Rust version policy

This crate's minimum supported `rustc` version is `1.0.0`.  Compatibility is
its entire reason for existence, so this crate will be extremely conservative
about raising this requirement.  If this is ever deemed necessary, it will be
treated as a major breaking change for semver purposes.


## License

This project is licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or
   http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or
   http://opensource.org/licenses/MIT)

at your option.
