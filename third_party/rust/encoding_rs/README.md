# encoding_rs

[![Build Status](https://travis-ci.org/hsivonen/encoding_rs.svg?branch=master)](https://travis-ci.org/hsivonen/encoding_rs)
[![crates.io](https://meritbadge.herokuapp.com/encoding_rs)](https://crates.io/crates/encoding_rs)
[![docs.rs](https://docs.rs/encoding_rs/badge.svg)](https://docs.rs/encoding_rs/)
[![Apache 2 / MIT dual-licensed](https://img.shields.io/badge/license-Apache%202%20%2F%20MIT-blue.svg)](https://github.com/hsivonen/encoding_rs/blob/master/COPYRIGHT)

encoding_rs an implementation of the (non-JavaScript parts of) the
[Encoding Standard](https://encoding.spec.whatwg.org/) written in Rust and
used in Gecko (starting with Firefox 56).

Additionally, the `mem` module provides various operations for dealing with
in-RAM text (as opposed to data that's coming from or going to an IO boundary).
The `mem` module is a module instead of a separate crate due to internal
implementation detail efficiencies.

## Functionality

Due to the Gecko use case, encoding_rs supports decoding to and encoding from
UTF-16 in addition to supporting the usual Rust use case of decoding to and
encoding from UTF-8. Additionally, the API has been designed to be FFI-friendly
to accommodate the C++ side of Gecko.

Specifically, encoding_rs does the following:

* Decodes a stream of bytes in an Encoding Standard-defined character encoding
  into valid aligned native-endian in-RAM UTF-16 (units of `u16` / `char16_t`).
* Encodes a stream of potentially-invalid aligned native-endian in-RAM UTF-16
  (units of `u16` / `char16_t`) into a sequence of bytes in an Encoding
  Standard-defined character encoding as if the lone surrogates had been
  replaced with the REPLACEMENT CHARACTER before performing the encode.
  (Gecko's UTF-16 is potentially invalid.)
* Decodes a stream of bytes in an Encoding Standard-defined character
  encoding into valid UTF-8.
* Encodes a stream of valid UTF-8 into a sequence of bytes in an Encoding
  Standard-defined character encoding. (Rust's UTF-8 is guaranteed-valid.)
* Does the above in streaming (input and output split across multiple
  buffers) and non-streaming (whole input in a single buffer and whole
  output in a single buffer) variants.
* Avoids copying (borrows) when possible in the non-streaming cases when
  decoding to or encoding from UTF-8.
* Resolves textual labels that identify character encodings in
  protocol text into type-safe objects representing the those encodings
  conceptually.
* Maps the type-safe encoding objects onto strings suitable for
  returning from `document.characterSet`.
* Validates UTF-8 (in common instruction set scenarios a bit faster for Web
  workloads than the standard library; hopefully will get upstreamed some
  day) and ASCII.

Additionally, `encoding_rs::mem` does the following:

* Checks if a byte buffer contains only ASCII.
* Checks if a potentially-invalid UTF-16 buffer contains only Basic Latin (ASCII).
* Checks if a valid UTF-8, potentially-invalid UTF-8 or potentially-invalid UTF-16
  buffer contains only Latin1 code points (below U+0100).
* Checks if a valid UTF-8, potentially-invalid UTF-8 or potentially-invalid UTF-16
  buffer or a code point or a UTF-16 code unit can trigger right-to-left behavior
  (suitable for checking if the Unicode Bidirectional Algorithm can be optimized
  out).
* Combined versions of the above two checks.
* Converts valid UTF-8, potentially-invalid UTF-8 and Latin1 to UTF-16.
* Converts potentially-invalid UTF-16 and Latin1 to UTF-8.
* Converts UTF-8 and UTF-16 to Latin1 (if in range).
* Finds the first invalid code unit in a buffer of potentially-invalid UTF-16.
* Makes a mutable buffer of potential-invalid UTF-16 contain valid UTF-16.
* Copies ASCII from one buffer to another up to the first non-ASCII byte.
* Converts ASCII to UTF-16 up to the first non-ASCII byte.
* Converts UTF-16 to ASCII up to the first non-Basic Latin code unit.

## Licensing

Please see the file named
[COPYRIGHT](https://github.com/hsivonen/encoding_rs/blob/master/COPYRIGHT).

## API Documentation

Generated [API documentation](https://docs.rs/encoding_rs/) is available
online.

## C and C++ bindings

An FFI layer for encoding_rs is available as a
[separate crate](https://github.com/hsivonen/encoding_c). The crate comes
with a [demo C++ wrapper](https://github.com/hsivonen/encoding_c/blob/master/include/encoding_rs_cpp.h)
using the C++ standard library and [GSL](https://github.com/Microsoft/GSL/) types.

For the Gecko context, there's a
[C++ wrapper using the MFBT/XPCOM types](https://searchfox.org/mozilla-central/source/intl/Encoding.h#100).

These bindings do not cover the `mem` module.

## Sample programs

* [Rust](https://github.com/hsivonen/recode_rs)
* [C](https://github.com/hsivonen/recode_c)
* [C++](https://github.com/hsivonen/recode_cpp)

## Optional features

There are currently three optional cargo features:

### `simd-accel`

Enables SSE2 acceleration on x86 and x86_64 and NEON acceleration on Aarch64
and ARMv7. Requires nightly Rust. _Enabling this cargo feature is recommended
when building for x86, x86_64, ARMv7 or Aarch64 on nightly Rust._ The intention
is for the functionality enabled by this feature to become the normal
on-by-default behavior once
[portable SIMD](https://github.com/rust-lang/rfcs/pull/2366) becames available
on all Rust release channels.

Enabling this feature breaks the build unless the target is x86 with SSE2
(Rust's default 32-bit x86 target, `i686`, has SSE2, but Linux distros may
use an x86 target without SSE2, i.e. `i586` in `rustup` terms), ARMv7 or
thumbv7 with NEON (`-C target_feature=+neon`), x86_64 or Aarch64.

### `serde`

Enables support for serializing and deserializing `&'static Encoding`-typed
struct fields using [Serde][1].

[1]: https://serde.rs/

### `less-slow-kanji-encode`

Makes JIS X 0208 Level 1 Kanji (the most common Kanji in Shift_JIS, EUC-JP and
ISO-2022-JP) encode less slow (binary search instead of linear search) at the
expense of binary size. (Does _not_ affect decode speed.)

Not used by Firefox.

### `less-slow-gb-hanzi-encode`

Makes GB2312 Level 1 Hanzi (the most common Hanzi in gb18030 and GBK) encode
less slow (binary search instead of linear search) at the expense of binary
size. (Does _not_ affect decode speed.)

Not used by Firefox.

### `less-slow-big5-hanzi-encode`

Makes Big5 Level 1 Hanzi (the most common Hanzi in Big5) encode less slow
(binary search instead of linear search) at the expense of binary size. (Does
_not_ affect decode speed.)

Not used by Firefox.

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

In the interest of binary size, by default, encoding_rs does not have any
encode-specific data tables. Therefore, encoders search the decode-optimized
data tables. This is a linear search in most cases. As a result, encode to
legacy encodings varies from slow to extremely slow relative to other
libraries. Still, with realistic work loads, this seemed fast enough
not to be user-visibly slow on Raspberry Pi 3 (which stood in for a phone
for testing) in the Web-exposed encoder use cases.

See the cargo features above for optionally making Kanji and Hanzi legacy
encode a bit less slow.

Actually fast options for legacy encode may be added in the future, but there
do not appear to be pressing use cases.

A framework for measuring performance is [available separately][2].

[2]: https://github.com/hsivonen/encoding_bench/

## Rust Version Compatibility

It is a goal to support the latest stable Rust, the latest nightly Rust and
the version of Rust that's used for Firefox Nightly (currently 1.25.0).
These are tested on Travis.

Additionally, beta and the oldest known to work Rust version (currently
1.21.0) are tested on Travis. The oldest Rust known to work is tested as
a canary so that when the oldest known to work no longer works, the change
can be documented here. At this time, there is no firm commitment to support
a version older than what's required by Firefox. The oldest supported Rust
is expected to move forward rapidly when `stdsimd` can replace the `simd`
crate without performance regression.

## Compatibility with rust-encoding

A compatibility layer that implements the rust-encoding API on top of
encoding_rs is
[provided as a separate crate](https://github.com/hsivonen/encoding_rs_compat)
(cannot be uploaded to crates.io). The compatibility layer was originally
written with the assuption that Firefox would need it, but it is not currently
used in Firefox.

## Roadmap

- [x] Design the low-level API.
- [x] Provide Rust-only convenience features.
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
- [ ] ~Parallelize UTF-8 validation using [Rayon](https://github.com/nikomatsakis/rayon).~
      (This turned out to be a pessimization in the ASCII case due to memory bandwidth reasons.)
- [x] Provide an XPCOM/MFBT-flavored C++ API.
- [ ] Investigate accelerating single-byte encode with a single fast-tracked
      range per encoding.
- [x] Replace uconv with encoding_rs in Gecko.
- [x] Implement the rust-encoding API in terms of encoding_rs.
- [x] Add SIMD acceleration for Aarch64.
- [x] Investigate the use of NEON on 32-bit ARM.
- [ ] Investigate Björn Höhrmann's lookup table acceleration for UTF-8 as
      adapted to Rust in rust-encoding.
- [ ] Add actually fast CJK encode options.

## Release Notes

### 0.8.3

* Removed an `#[inline(never)]` annotation that was not meant for release.

### 0.8.2

* Made non-ASCII UTF-16 to UTF-8 encode faster by manually omitting bound
  checks and manually adding branch prediction annotations.

### 0.8.1

* Tweaked loop unrolling and memory alignment for SSE2 conversions between
  UTF-16 and Latin1 in the `mem` module to increase the performance when
  converting long buffers.

### 0.8.0

* Changed the minimum supported version of Rust to 1.21.0 (semver breaking
  change).
* Flipped around the defaults vs. optional features for controlling the size
  vs. speed trade-off for Kanji and Hanzi legacy encode (semver breaking
  change).
* Added NEON support on ARMv7.
* SIMD-accelerated x-user-defined to UTF-16 decode.
* Made UTF-16LE and UTF-16BE decode a lot faster (including SIMD
  acceleration).

### 0.7.2

* Add the `mem` module.
* Refactor SIMD code which can affect performance outside the `mem`
  module.

### 0.7.1

* When encoding from invalid UTF-16, correctly handle U+DC00 followed by
  another low surrogate.

### 0.7.0

* [Make `replacement` a label of the replacement
  encoding.](https://github.com/whatwg/encoding/issues/70) (Spec change.)
* Remove `Encoding::for_name()`. (`Encoding::for_label(foo).unwrap()` is
  now close enough after the above label change.)
* Remove the `parallel-utf8` cargo feature.
* Add optional Serde support for `&'static Encoding`.
* Performance tweaks for ASCII handling.
* Performance tweaks for UTF-8 validation.
* SIMD support on aarch64.

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
