# proc-macro2

[![Build Status](https://api.travis-ci.org/alexcrichton/proc-macro2.svg?branch=master)](https://travis-ci.org/alexcrichton/proc-macro2)
[![Latest Version](https://img.shields.io/crates/v/proc-macro2.svg)](https://crates.io/crates/proc-macro2)
[![Rust Documentation](https://img.shields.io/badge/api-rustdoc-blue.svg)](https://docs.rs/proc-macro2)

A small shim over the `proc_macro` crate in the compiler intended to multiplex
the stable interface as of 1.15.0 and the interface as of 1.30.0.

New features added in Rust 1.30.0 include:

* Span information on tokens
* No need to go in/out through strings
* Structured input/output

Libraries ported to `proc_macro2` can retain support for older compilers while
continuing to get all the nice benefits of using a 1.30.0+ compiler.

## Usage

This crate compiles on all 1.15.0+ stable compilers and usage looks like:

```toml
[dependencies]
proc-macro2 = "0.4"
```

followed by

```rust
extern crate proc_macro;
extern crate proc_macro2;

#[proc_macro_derive(MyDerive)]
pub fn my_derive(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input: proc_macro2::TokenStream = input.into();

    let output: proc_macro2::TokenStream = {
        /* transform input */
    };

    output.into()
}
```

The 1.30.0 compiler is automatically detected and its interfaces are used when
available.

## Unstable Features

`proc-macro2` supports exporting some methods from `proc_macro` which are
currently highly unstable, and are not stabilized in the first pass of
`proc_macro` stabilizations. These features are not exported by default. Minor
versions of `proc-macro2` may make breaking changes to them at any time.

To enable these features, the `procmacro2_semver_exempt` config flag must be
passed to rustc.

```
RUSTFLAGS='--cfg procmacro2_semver_exempt' cargo build
```

Note that this must not only be done for your crate, but for any crate that
depends on your crate. This infectious nature is intentional, as it serves as a
reminder that you are outside of the normal semver guarantees.

# License

This project is licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or
   http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or
   http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in Serde by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
