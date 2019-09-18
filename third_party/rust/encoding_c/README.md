# encoding_c

[![crates.io](https://meritbadge.herokuapp.com/encoding_c)](https://crates.io/crates/encoding_c)
[![docs.rs](https://docs.rs/encoding_c/badge.svg)](https://docs.rs/encoding_c/)
[![Apache 2 / MIT dual-licensed](https://img.shields.io/badge/license-Apache%202%20%2F%20MIT-blue.svg)](https://github.com/hsivonen/encoding_c/blob/master/COPYRIGHT)

encoding_c is an FFI wrapper for [encoding_rs](https://github.com/hsivonen/encoding_rs).

## Bindings for `encoding_rs::mem`

See the [`encoding_c_mem` crate](https://crates.io/crates/encoding_c_mem)
for bindings for `encoding_rs::mem`.

## Licensing

Please see the file named
[COPYRIGHT](https://github.com/hsivonen/encoding_c/blob/master/COPYRIGHT).

## No Unwinding Support!

This crate is meant for use in binaries compiled with `panic = 'abort'`, which
is _required_ for correctness! Unwinding across FFI is Undefined Behavior, and
this crate does nothing to try to prevent unwinding across the FFI if
compiled with unwinding enabled.

## C/C++ Headers

`include/encoding_rs.h` and `include/encoding_rs_statics.h` are needed for C
usage.

`include/encoding_rs_cpp.h` is a sample C++ API built on top of the C API using
GSL and the C++ standard library. Since C++ project typically roll their own
string classes, etc., it's probably necessary for C++ projects to manually
adapt the header to their replacements of standard-library types.

There's a [write-up](https://hsivonen.fi/modern-cpp-in-rust/) about the C++
wrappers.

## Release Notes

### 0.9.4

* Fix bogus C header.

### 0.9.3

* Fix bogus C++ header.

### 0.9.2

* Wrap `Decoder::latin1_byte_compatible_up_to`.

### 0.9.1

* Wrap `Encoding::is_single_byte()`.
* Pass through new feature flags introduced in encoding_rs 0.8.11.

### 0.9.0

* Update to encoding_rs 0.8.0.

### 0.8.0

* Update to encoding_rs 0.7.0.
* Drop `encoding_for_name()`.
* Deal correctly with the `data()` method of `gsl::span` returning `nullptr`.

### 0.7.6

* Rename `ENCODING_RS_NON_NULL_CONST_ENCODING_PTR` to
  `ENCODING_RS_NOT_NULL_CONST_ENCODING_PTR`. (Not a breaking change,
  because defining that macro broke the build previously, so the
  macro couldn't have been used.)
* Use the macro only for statics and not for return values.

### 0.7.5

* Annotate the encoding pointers that should be wrapped with a
  same-representation not-null type in C++ as
  `ENCODING_RS_NON_NULL_CONST_ENCODING_PTR`.

### 0.7.4

* Wrap `has_pending_state()`.

### 0.7.3

* Use C preprocessor definitions for encoding constant declarations.

### 0.7.2

* Parametrize the struct type names behind C preprocessor definitions.
* Leave it to the user to provide `char16_t`. Avoid including a header for it.

### 0.7.1

* Fix documentation for pointers that get used in
  `std::slice::from_raw_parts()`.

### 0.7.0

* Map `None` to `SIZE_MAX` in the max length calculation functions.

### 0.6.0

* Check in the `cheddar`-generated header and comment out the `cheddar`-using
  `build.rs`.

### 0.5.0

* Initial release of encoding_c. (I.e. first release with FFI in a distinct
  crate.)

