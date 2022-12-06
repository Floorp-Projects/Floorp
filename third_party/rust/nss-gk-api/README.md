# (UNSTABLE) Gecko API for NSS

nss-gk-api is intended to provide a safe and idiomatic Rust interface to NSS.  It is based on code from neqo-crypto, but has been factored out of mozilla-central so that it can be used in standalone applications and libraries such as authenticator-rs. That said, it is *primarily* for use in Gecko, and will not be extended to support arbitrary use cases.

This is work in progress and major changes are expected. In particular, a new version of the `nss-sys` crate will be factored out of this crate.

