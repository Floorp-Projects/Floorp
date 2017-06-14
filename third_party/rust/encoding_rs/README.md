# encoding_rs

[![Build Status](https://travis-ci.org/hsivonen/encoding_rs.svg?branch=master)](https://travis-ci.org/hsivonen/encoding_rs)
[![crates.io](https://meritbadge.herokuapp.com/encoding_rs)](https://crates.io/crates/encoding_rs)
[![docs.rs](https://docs.rs/encoding_rs/badge.svg)](https://docs.rs/encoding_rs/)
[![Apache 2 / MIT dual-licensed](https://img.shields.io/badge/license-Apache%202%20%2F%20MIT-blue.svg)](https://github.com/hsivonen/encoding_rs/blob/master/COPYRIGHT)

encoding_rs aspires to become an implementation of the
[Encoding Standard](https://encoding.spec.whatwg.org/) that

1. Is written in Rust.
2. Is suitable for use in Gecko as a replacement of uconv. (I.e. supports
   decoding to UTF-16 and encoding from UTF-16.)
3. Is suitable for use in Rust code (both in Gecko and independently of Gecko).
   (I.e. supports decoding to UTF-8 and encoding from UTF-8 and provides an API
   compatible with at least the most common ways of using
   [rust-encoding](https://github.com/lifthrasiir/rust-encoding/).)

## Licensing

Please see the file named
[COPYRIGHT](https://github.com/hsivonen/encoding_rs/blob/master/COPYRIGHT).

## API Documentation

Generated [API documentation](https://docs.rs/encoding_rs/) is available
online.

## Design

For design considerations, please see the associated [technical proposal to
rewrite uconv in Rust](https://docs.google.com/document/d/13GCbdvKi83a77ZcKOxaEteXp1SOGZ_9Fmztb9iX22v0/edit#).

## Performance goals

For decoding to UTF-16, the goal is to perform at least as well as Gecko's old
uconv. For decoding to UTF-8, the goal is to perform at least as well as
rust-encoding.

Encoding to UTF-8 should be fast. (UTF-8 to UTF-8 encode should be equivalent
to `memcpy` and UTF-16 to UTF-8 should be fast.)

Speed is a non-goal when encoding to legacy encodings. Encoding to legacy
encodings should not be optimized for speed at the expense of code size as long
as form submission and URL parsing in Gecko don't become noticeably too slow
in real-world use.

A framework for measuring performance is [available separately][1].

[1]: https://github.com/hsivonen/encoding_bench/

## C binding

An FFI layer for encoding_rs is available as a
[separate crate](https://github.com/hsivonen/encoding_c).

## Compatibility with rust-encoding

A compatibility layer that implements the rust-encoding API on top of
encoding_rs is
[provided as a separate crate](https://github.com/hsivonen/encoding_rs_compat)
(cannot be uploaded to crates.io).

## Roadmap

- [x] Design the low-level API.
- [x] Provide Rust-only convenience features (some BOM sniffing variants still
      TODO).
- [x] Provide an stl/gsl-flavored C++ API.
- [x] Implement all decoders and encoders.
- [x] Add unit tests for all decoders and encoders.
- [x] Finish BOM sniffing variants in Rust-only convenience features.
- [x] Document the API.
- [x] Publish the crate on crates.io.
- [x] Create a solution for measuring performance.
- [x] Accelerate ASCII conversions using SSE2 on x86.
- [x] Accelerate ASCII conversions using ALU register-sized operations on
      non-x86 architectures (process an `usize` instead of `u8` at a time).
- [x] Split FFI into a separate crate so that the FFI doesn't interfere with
      LTO in pure-Rust usage.
- [x] Compress CJK indices by making use of sequential code points as well
      as Unicode-ordered parts of indices.
- [x] Make lookups by label or name use binary search that searches from the
      end of the label/name to the start.
- [x] Make labels with non-ASCII bytes fail fast.
- [x] Parallelize UTF-8 validation using [Rayon](https://github.com/nikomatsakis/rayon).
- [x] Provide an XPCOM/MFBT-flavored C++ API.
- [ ] Investigate accelerating single-byte encode with a single fast-tracked
      range per encoding.
- [ ] Replace uconv with encoding_rs in Gecko.
- [x] Implement the rust-encoding API in terms of encoding_rs.
- [ ] Investigate the use of NEON on newer ARM CPUs that have a lesser penalty
      on data flow from NEON to ALU registers.
- [ ] Investigate Björn Höhrmann's lookup table acceleration for UTF-8 as
      adapted to Rust in rust-encoding.

## Release Notes

### 0.6.11

* Make `Encoder::has_pending_state()` public.
* Update the `simd` crate dependency to 0.2.0.

### 0.6.10

* Reserve enough space for NCRs when encoding to ISO-2022-JP.
* Correct max length calculations for multibyte decoders.
* Correct max length calculations before BOM sniffing has been
  performed.
* Correctly calculate max length when encoding from UTF-16 to GBK.

### 0.6.9

* [Don't prepend anything when gb18030 range decode
  fails](https://github.com/whatwg/encoding/issues/110). (Spec change.)

### 0.6.8

* Correcly handle the case where the first buffer contains potentially
  partial BOM and the next buffer is the last buffer.
* Decode byte `7F` correctly in ISO-2022-JP.
* Make UTF-16 to UTF-8 encode write closer to the end of the buffer.
* Implement `Hash` for `Encoding`.

### 0.6.7

* [Map half-width katakana to full-width katana in ISO-2022-JP
  encoder](https://github.com/whatwg/encoding/issues/105). (Spec change.)
* Give `InputEmpty` correct precedence over `OutputFull` when encoding
  with replacement and the output buffer passed in is too short or the
  remaining space in the output buffer is too small after a replacement.

### 0.6.6

* Correct max length calculation when a partial BOM prefix is part of
  the decoder's state.

### 0.6.5

* Correct max length calculation in various encoders.
* Correct max length calculation in the UTF-16 decoder.
* Derive `PartialEq` and `Eq` for the `CoderResult`, `DecoderResult`
  and `EncoderResult` types.

### 0.6.4

* Avoid panic when encoding with replacement and the destination buffer is
  too short to hold one numeric character reference.

### 0.6.3

* Add support for 32-bit big-endian hosts. (For real this time.)

### 0.6.2

* Fix a panic from subslicing with bad indices in
  `Encoder::encode_from_utf16`. (Due to an oversight, it lacked the fix that
  `Encoder::encode_from_utf8` already had.)
* Micro-optimize error status accumulation in non-streaming case.

### 0.6.1

* Avoid panic near integer overflow in a case that's unlikely to actually
  happen.
* Address Clippy lints.

### 0.6.0

* Make the methods for computing worst-case buffer size requirements check
  for integer overflow.
* Upgrade rayon to 0.7.0.

### 0.5.1

* Reorder methods for better documentation readability.
* Add support for big-endian hosts. (Only 64-bit case actually tested.)
* Optimize the ALU (non-SIMD) case for 32-bit ARM instead of x86_64.

### 0.5.0

* Avoid allocating an excessively long buffers in non-streaming decode.
* Fix the behavior of ISO-2022-JP and replacement decoders near the end of the
  output buffer.
* Annotate the result structs with `#[must_use]`.

### 0.4.0

* Split FFI into a separate crate.
* Performance tweaks.
* CJK binary size and encoding performance changes.
* Parallelize UTF-8 validation in the case of long buffers (with optional
  feature `parallel-utf8`).
* Borrow even with ISO-2022-JP when possible.

### 0.3.2

* Fix moving pointers to alignment in ALU-based ASCII acceleration.
* Fix errors in documentation and improve documentation.

### 0.3.1

* Fix UTF-8 to UTF-16 decode for byte sequences beginning with 0xEE.
* Make UTF-8 to UTF-8 decode SSE2-accelerated when feature `simd-accel` is used.
* When decoding and encoding ASCII-only input from or to an ASCII-compatible
  encoding using the non-streaming API, return a borrow of the input.
* Make encode from UTF-16 to UTF-8 faster.

### 0.3

* Change the references to the instances of `Encoding` from `const` to `static`
  to make the referents unique across crates that use the refernces.
* Introduce non-reference-typed `FOO_INIT` instances of `Encoding` to allow
  foreign crates to initialize `static` arrays with references to `Encoding`
  instances even under Rust's constraints that prohibit the initialization of
  `&'static Encoding`-typed array items with `&'static Encoding`-typed
  `statics`.
* Document that the above two points will be reverted if Rust changes `const`
  to work so that cross-crate usage keeps the referents unique.
* Return `Cow`s from Rust-only non-streaming methods for encode and decode.
* `Encoding::for_bom()` returns the length of the BOM.
* ASCII-accelerated conversions for encodings other than UTF-16LE, UTF-16BE,
  ISO-2022-JP and x-user-defined.
* Add SSE2 acceleration behind the `simd-accel` feature flag. (Requires
  nightly Rust.)
* Fix panic with long bogus labels.
* Map [0xCA to U+05BA in windows-1255](https://github.com/whatwg/encoding/issues/73).
  (Spec change.)
* Correct the [end of the Shift_JIS EUDC range](https://github.com/whatwg/encoding/issues/53).
  (Spec change.)

### 0.2.4

* Polish FFI documentation.

### 0.2.3

* Fix UTF-16 to UTF-8 encode.

### 0.2.2

* Add `Encoder.encode_from_utf8_to_vec_without_replacement()`.

### 0.2.1

* Add `Encoding.is_ascii_compatible()`.

* Add `Encoding::for_bom()`.

* Make `==` for `Encoding` use name comparison instead of pointer comparison,
  because uses of the encoding constants in different crates result in
  different addresses and the constant cannot be turned into statics without
  breaking other things.

### 0.2.0

The initial release.
