# Changelog

## Unreleased Changes

## v3.0.0

- The cryptography library used is now configurable.
    - By default `ring` is used (the `use_ring` feature).
    - You can use the `use_openssl` feature to use openssl instead
        - e.g. in your Cargo.toml:
        ```toml
        [dependencies.hawk]
        version = "..."
        features = ["use_openssl"]
        default-features = false
        ```
    - You can use neither and provide your own implementation using the functions in
      `hawk::crypto` if neither feature is enabled.
    - Note that enabling both `use_ring` and `use_openssl` will cause a build
      failure.

- BREAKING: Many functions that previously returned `T` now return `hawk::Result<T>`.
    - Specifically, `PayloadHasher::{hash,update,finish}`, `Key::{new,sign}`.

- BREAKING: `hawk::SHA{256,384,512}` are now `const` `DigestAlgorithm`s and not
  aliases for `ring::Algorithm`

- BREAKING: `Key::new` now takes a `DigestAlgorithm` and not a
  `&'static ring::Algorithm`.
    - If you were passing e.g. `&hawk::SHA256`, you probably just need
      to pass `hawk::SHA256` now instead.

- BREAKING (though unlikely): `Error::Rng` has been removed, and `Error::Crypto` added

