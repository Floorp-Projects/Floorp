# xmldecl

[![crates.io](https://img.shields.io/crates/v/xmldecl.svg)](https://crates.io/crates/xmldecl)
[![docs.rs](https://docs.rs/xmldecl/badge.svg)](https://docs.rs/xmldecl/)
[![Apache 2 OR MIT dual-licensed](https://img.shields.io/badge/license-Apache%202%20%2F%20MIT-blue.svg)](https://github.com/hsivonen/xmldecl/blob/main/COPYRIGHT)

`xmldecl::parse()` extracts an encoding from an ASCII-based bogo-XML
declaration in `text/html` in a Web-compatible way.

See https://github.com/whatwg/html/pull/1752

## Licensing

Please see the file named
[COPYRIGHT](https://github.com/hsivonen/xmldecl/blob/main/COPYRIGHT).

## Documentation

Generated [API documentation](https://docs.rs/xmldecl/) is available
online.

## Minimum Supported Rust Version

MSRV: 1.51.0

## Release Notes

### 0.2.0

* Remove the 1024-byte limit.

### 0.1.1

* Map UTF-16LE and UTF-16BE to UTF-8 as in `meta`.

### 0.1.0

* The initial release.
