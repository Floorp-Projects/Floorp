# rustc-demangle

Demangling for Rust symbols, written in Rust.

[Documentation](https://docs.rs/rustc-demangle)

## Usage

You can add this as a dependency via your `Cargo.toml`

```toml
[dependencies]
rustc-demangle = "0.1"
```

and then be sure to check out the [crate
documentation](https://docs.rs/rustc-demangle) for usage.

## Usage from non-Rust languages

You can also use this crate from other languages via the C API wrapper in the
`crates/capi` directory. This can be build with:

```sh
$ cargo build -p rustc-demangle-capi --release
```

You'll then find `target/release/librustc_demangle.a` and
`target/release/librustc_demangle.so` (or a different name depending on your
platform). These objects implement the interface specified in
`crates/capi/include/rustc_demangle.h`.

# License

This project is licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or
   http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or
   http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in rustc-demangle you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
