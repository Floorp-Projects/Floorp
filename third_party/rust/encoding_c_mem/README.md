# encoding_c_mem

[![crates.io](https://meritbadge.herokuapp.com/encoding_c_mem)](https://crates.io/crates/encoding_c_mem)
[![docs.rs](https://docs.rs/encoding_c_mem/badge.svg)](https://docs.rs/encoding_c_mem/)
[![Apache 2 / MIT dual-licensed](https://img.shields.io/badge/license-Apache%202%20%2F%20MIT-blue.svg)](https://github.com/hsivonen/encoding_c_mem/blob/master/COPYRIGHT)

encoding_c_mem is an FFI wrapper for the `mem` module of [encoding_rs](https://github.com/hsivonen/encoding_rs).

## Licensing

Please see the file named
[COPYRIGHT](https://github.com/hsivonen/encoding_c_mem/blob/master/COPYRIGHT).

## No Unwinding Support!

This crate is meant for use in binaries compiled with `panic = 'abort'`, which
is _required_ for correctness! Unwinding across FFI is Undefined Behavior, and
this crate does nothing to try to prevent unwinding across the FFI if
compiled with unwinding enabled.

## Release Notes

### 0.2.5

* Specify a `links` value in the Cargo manifest.
* Emit an `include_dir` variable from build script so that other build scripts
  depending on this crate can rely on it.

### 0.2.4

* Documentation-only fix.

### 0.2.3

* Documentation-only fix.

### 0.2.2

* Wrap `convert_utf8_to_utf16_without_replacement`, `utf8_latin1_up_to`,
  and `str_latin1_up_to`.

### 0.2.1

* Fix a typo in README.

### 0.2.0

* Use `char` instead of `uint8_t` for 8-bit-unit text in C and C++.

### 0.1.1

* Add include guard to the C header.

### 0.1.0

* Initial release of encoding_c_mem.
