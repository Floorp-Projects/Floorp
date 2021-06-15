# rust-ece &emsp; [![Build Status]][circleci] [![Latest Version]][crates.io]

[Build Status]: https://circleci.com/gh/mozilla/rust-ece.svg?style=svg
[circleci]: https://circleci.com/gh/mozilla/rust-ece
[Latest Version]: https://img.shields.io/crates/v/ece.svg
[crates.io]: https://crates.io/crates/ece

*This crate has not been security reviewed yet, use at your own risk ([tracking issue](https://github.com/mozilla/rust-ece/issues/18))*.

[ece](https://crates.io/crates/ece) is a Rust implementation of the HTTP Encrypted Content-Encoding standard (RFC 8188). It is a port of the [ecec](https://github.com/web-push-libs/ecec) C library.  
This crate is destined to be used by higher-level Web Push libraries, both on the server and the client side.  

[Documentation](https://docs.rs/ece/)

## Cryptographic backends

This crate is designed to be used with different crypto backends. At the moment only [openssl](https://github.com/sfackler/rust-openssl) is supported.

## Implemented schemes

Currently, two HTTP ece schemes are available to consumers of the crate:
- The newer [RFC8188](https://tools.ietf.org/html/rfc8188) `aes128gcm` standard.
- The legacy [draft-03](https://tools.ietf.org/html/draft-ietf-httpbis-encryption-encoding-03) `aesgcm` scheme.

## Release process

We use [`cargo-release`](https://crates.io/crates/cargo-release) to manage releases. To cut a new release,
make sure you have it installed and then:

* Run `cargo release --dry-run -vv [major|minor|patch]` and check that the things it's proposing to do
  seem sensible.
* Run `cargo release [major|minor|patch]` to prepare, commit, tag and publish the release.

