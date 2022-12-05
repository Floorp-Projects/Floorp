# Oblivious HTTP

This is a rust implementation of [Oblivious
HTTP](https://ietf-wg-ohai.github.io/oblivious-http/draft-ietf-ohai-ohttp.html).

This work is undergoing active revision in the IETF and so are these
implementations.  Use at your own risk.

This crate uses either [hpke](https://github.com/rozbb/rust-hpke) or
[NSS](https://firefox-source-docs.mozilla.org/security/nss/index.html) for
cryptographic primitives.


## Using

The API documentation is currently sparse, but the API is fairly small and
descriptive.

The `ohttp` crate has the following features:

- `client` enables the client-side processing of oblivious HTTP messages:
  encrypting requests and decrypting responses.  This is enabled by default.

- `server` enables the server-side processing of oblivious HTTP messages:
  decrypting requests and encrypting responses.  This is enabled by default.

- `rust-hpke` selects the [hpke](https://github.com/rozbb/rust-hpke) crate for
  HPKE encryption.  This is enabled by default and cannot be enabled at the same
  time as `nss`.

- `nss` selects
  [NSS](https://firefox-source-docs.mozilla.org/security/nss/index.html).  This is
  disabled by default and cannot be enabled at the same time as `rust-hpke`.
