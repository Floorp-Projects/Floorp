# cstr

[![CI](https://github.com/upsuper/cstr/workflows/CI/badge.svg)](https://github.com/upsuper/cstr/actions)
[![Crates.io](https://img.shields.io/crates/v/cstr.svg)](https://crates.io/crates/cstr)
[![Docs](https://docs.rs/cstr/badge.svg)](https://docs.rs/cstr)

<!-- cargo-sync-readme start -->

A macro for getting `&'static CStr` from literal or identifier.

This macro checks whether the given literal is valid for `CStr`
at compile time, and returns a static reference of `CStr`.

This macro can be used to to initialize constants on Rust 1.64 and above.

## Example

```rust
use cstr::cstr;
use std::ffi::CStr;

let test = cstr!(b"hello\xff");
assert_eq!(test, CStr::from_bytes_with_nul(b"hello\xff\0").unwrap());
let test = cstr!("hello");
assert_eq!(test, CStr::from_bytes_with_nul(b"hello\0").unwrap());
let test = cstr!(hello);
assert_eq!(test, CStr::from_bytes_with_nul(b"hello\0").unwrap());
```

<!-- cargo-sync-readme end -->
