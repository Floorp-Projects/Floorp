# The Crates.io Version Can No Longer Be Updated!

The original maintainer is out of contact, and the new maintainers (the Portable SIMD Project Group) do not have the appropriate crates.io permissions to issue updates.

We are aware that the version available on crates.io is currently broken, and will not build.

If you need to continue to use the crate, we have published a "next version" under an alternative name.

Adjust your `[dependencies]` section of `Cargo.toml` to be the following:
```toml
packed_simd = { version = "0.3.8", package = "packed_simd_2" }
```

# `Simd<[T; N]>`

## Implementation of [Rust RFC #2366: `std::simd`][rfc2366]

[![Travis-CI Status]][travis] <!-- [![Appveyor Status]][appveyor] --> [![Latest Version]][crates.io] [![docs]][master_docs]

**WARNING**: this crate only supports the most recent nightly Rust toolchain
and will be superseded by [stdsimd](https://github.com/rust-lang/stdsimd).

## Documentation

* [API docs (`master` branch)][master_docs]
* [Performance guide][perf_guide]
* [API docs (`docs.rs`)][docs.rs]
* [RFC2366 `std::simd`][rfc2366]: - contains motivation, design rationale,
  discussion, etc.

## Examples

Most of the examples come with both a scalar and a vectorized implementation.

* [`aobench`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/aobench)
* [`fannkuch_redux`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/fannkuch_redux)
* [`matrix inverse`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/matrix_inverse)
* [`mandelbrot`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/mandelbrot)
* [`n-body`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/nbody)
* [`options_pricing`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/options_pricing)
* [`spectral_norm`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/spectral_norm)
* [`triangle transform`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/triangle_xform)
* [`stencil`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/stencil)
* [`vector dot product`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/dot_product)

## Cargo features

* `into_bits` (default: disabled): enables `FromBits`/`IntoBits` trait
  implementations for the vector types. These allow reinterpreting the bits of a
  vector type as those of another vector type safely by just using the
  `.into_bits()` method.

## Performance

The following [ISPC] examples are also part of `packed_simd`'s
[`examples/`](https://github.com/rust-lang-nursery/packed_simd/tree/master/examples/)
directory, where `packed_simd`+[`rayon`][rayon] are used to emulate [ISPC]'s
Single-Program-Multiple-Data (SPMD) programming model. The performance results
on different hardware is shown in the `readme.md` of each example. The following
table summarizes the performance ranges, where `+` means speed-up and `-`
slowdown:

* `aobench`: `[-1.02x, +1.53x]`,
* `stencil`: `[+1.06x, +1.72x]`,
* `mandelbrot`: `[-1.74x, +1.2x]`,
* `options_pricing`:
   * `black_scholes`: `+1.0x`
   * `binomial_put`: `+1.4x`

 While SPMD is not the intended use case for `packed_simd`, it is possible to
 combine the library with [`rayon`][rayon] to poorly emulate [ISPC]'s SPMD programming
 model in Rust. Writing performant code is not as straightforward as with
 [ISPC], but with some care (e.g. see the [Performance Guide][perf_guide]) one
 can easily match and often out-perform [ISPC]'s "default performance".

## Platform support

The following table describes the supported platforms: `build` shows whether
the library compiles without issues for a given target, while `run` shows
whether the test suite passes for a given target.

| **Linux**                             | **build** | **run** |
|---------------------------------------|-----------|---------|
| `i586-unknown-linux-gnu`              | ✓         | ✗       |
| `i686-unknown-linux-gnu`              | ✓         | ✗       |
| `x86_64-unknown-linux-gnu`            | ✓         | ✓       |
| `arm-unknown-linux-gnueabi`           | ✗         | ✗       |
| `arm-unknown-linux-gnueabihf`         | ✓         | ✓       |
| `armv7-unknown-linux-gnueabi`         | ✓         | ✓       |
| `aarch64-unknown-linux-gnu`           | ✓         | ✓       |
| `mips-unknown-linux-gnu`              | ✓         | ✗       |
| `mipsel-unknown-linux-musl`           | ✓         | ✗       |
| `mips64-unknown-linux-gnuabi64`       | ✓         | ✗       |
| `mips64el-unknown-linux-gnuabi64`     | ✓         | ✗       |
| `powerpc-unknown-linux-gnu`           | ✗         | ✗       |
| `powerpc64-unknown-linux-gnu`         | ✗         | ✗       |
| `powerpc64le-unknown-linux-gnu`       | ✓         | ✓       |
| `s390x-unknown-linux-gnu`             | ✗         | ✗       |
| `sparc64-unknown-linux-gnu`           | ✓         | ✗       |
| `thumbv7neon-unknown-linux-gnueabihf` | ✓         | ✓       |
| **MacOSX**                            | **build** | **run** |
| `x86_64-apple-darwin`                 | ✓         | ✓       |
| **Android**                           | **build** | **run** |
| `x86_64-linux-android`                | ✓         | ✓       |
| `arm-linux-androideabi`               | ✓         | ✓       |
| `aarch64-linux-android`               | ✓         | ✓       |
| `thumbv7neon-linux-androideabi`       | ✗         | ✗       |
| **iOS**                               | **build** | **run** |
| `x86_64-apple-ios`                    | ✓         | ✗       |
| `aarch64-apple-ios`                   | ✓         | ✗       |


## Machine code verification

The
[`verify/`](https://github.com/rust-lang-nursery/packed_simd/tree/master/verify)
crate tests disassembles the portable packed vector APIs at run-time and
compares the generated machine code against the desired one to make sure that
this crate remains efficient.

## License

This project is licensed under either of

* [Apache License, Version 2.0](http://www.apache.org/licenses/LICENSE-2.0)
  ([LICENSE-APACHE](LICENSE-APACHE))

* [MIT License](http://opensource.org/licenses/MIT)
  ([LICENSE-MIT](LICENSE-MIT))

at your option.

## Contributing

We welcome all people who want to contribute.
Please see the [contributing instructions] for more information.

Contributions in any form (issues, pull requests, etc.) to this project
must adhere to Rust's [Code of Conduct].

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in `packed_simd` by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.

[travis]: https://travis-ci.com/rust-lang/packed_simd
[Travis-CI Status]: https://travis-ci.com/rust-lang/packed_simd.svg?branch=master
[appveyor]: https://ci.appveyor.com/project/gnzlbg/packed-simd
[Appveyor Status]: https://ci.appveyor.com/api/projects/status/hd7v9dvr442hgdix?svg=true
[Latest Version]: https://img.shields.io/crates/v/packed_simd_2.svg
[crates.io]: https://crates.io/crates/packed_simd_2
[docs]: https://docs.rs/packed_simd_2/badge.svg
[docs.rs]: https://docs.rs/packed_simd_2
[master_docs]: https://rust-lang-nursery.github.io/packed_simd/packed_simd_2/
[perf_guide]: https://rust-lang-nursery.github.io/packed_simd/perf-guide/
[rfc2366]: https://github.com/rust-lang/rfcs/pull/2366
[ISPC]: https://ispc.github.io/
[rayon]: https://crates.io/crates/rayon
[boost_license]: https://www.boost.org/LICENSE_1_0.txt
[SLEEF]: https://sleef.org/
[sleef_sys]: https://crates.io/crates/sleef-sys
[contributing instructions]: contributing.md
[Code of Conduct]: https://www.rust-lang.org/en-US/conduct.html
