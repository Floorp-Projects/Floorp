# blake2-rfc

This is a pure Rust implementation of BLAKE2 based on [RFC 7693].

[RFC 7693]: https://tools.ietf.org/html/rfc7693

## Design

This crate follow the common API design for streaming hash functions,
which has one state/context struct and three associated functions: one
to initialize the struct, one which is called repeatedly to process the
incoming data, and one to do the final processing and return the hash.
For the case where the full data is already in memory, there is a
convenience function which does these three steps in a single call.

This basic design was slightly adapted to make a better use of Rust's
characteristics: the finalization function consumes the struct, doing a
move instead of a borrow, so the struct cannot be accidentally used
after its internal state has been overwritten by the finalization.

To prevent timing attacks, it's important that the comparison of hash
values takes constant time. To make it easier to do the right thing, the
finalization function returns the result wrapped in a struct which does
a constant-time comparison by default. If a constant-time comparison is
not necessary, the hash result can easily be extracted from this struct.

## Limitations

A single BLAKE2b hash is limited to 16 exabytes, lower than its
theorical limit (but identical to the BLAKE2s theorical limit), due to
the use of a `u64` as the byte counter. This limit can be increased, if
necessary, after either the `extprim` crate (with its `u128` type) or
the `OverflowingOps` trait become usable with the "stable" Rust release.

This crate does not attempt to clear potentially sensitive data from its
work memory (which includes the state context, the stack, and processor
registers). To do so correctly without a heavy performance penalty would
require help from the compiler. It's better to not attempt to do so than
to present a false assurance.

## Non-RFC uses

This crate is limited to the features described in the RFC: only the
"digest length" and "key length" parameters can be used.

If you need to use other advanced BLAKE2 features, this crate has an
undocumented function to create a hashing context with an arbitrary
parameter block, and an undocumented function to finalize the last node
in tree hashing mode. You are responsible for creating a valid parameter
block, for hashing the padded key block if using keyed hashing, and for
calling the correct finalization function. The parameter block is not
validated by these functions.

## SIMD optimization

This crate has experimental support for explicit SIMD optimizations. It
requires nightly Rust due to the use of unstable features.

The following cargo features enable the explicit SIMD optimization:

* `simd` enables the explicit use of SIMD vectors instead of a plain
  struct
* `simd_opt` additionally enables the use of SIMD shuffles to implement
  some of the rotates
* `simd_asm` additionally enables the use of inline asm to implement
  some of the SIMD shuffles

While one might expect that each of these is faster than the previous
one, and that they are all faster than not enabling explicit SIMD
vectors, that's not always the case. It can vary depending on target
architecture and compiler options. If you need the extra speed from
these optimizations, benchmark each one (the `bench` feature enables
`cargo bench` in this crate, so you can use for instance `cargo bench
--features="bench simd_asm"`). They have currently been tuned for SSE2
(x86 and x86-64) and NEON (arm).

## `no_std` support

This crate links against the Rust standard library by default, to
provide implementations of `std::io::Write`. To build `no_std`, use
[`default-features = false`](http://doc.crates.io/manifest.html#rules).

## License

Licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally
submitted for inclusion in the work by you, as defined in the Apache-2.0
license, shall be dual licensed as above, without any additional terms or
conditions.
