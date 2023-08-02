//! [![github-img]][github-url] [![crates-img]][crates-url] [![docs-img]][docs-url]
//!
//! [github-url]: https://github.com/QnnOkabayashi/strck_ident
//! [crates-url]: https://crates.io/crates/strck_ident
//! [docs-url]: crate
//! [github-img]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-img]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-img]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K
//!
//! Checked owned and borrowed Unicode-based identifiers.
//!
//! # Overview
//!
//! [`strck`] is a crate for creating checked owned and borrowed strings with
//! arbitrary invariants as the type level. This crate extends `strct` by providing
//! [`Invariant`]s for [Unicode identifiers][unicode] and [Rust identifiers][rust].
//! In the future, this crate may support identifiers for other languages as well.
//!
//! This crate re-exports [`Check`], [`Ck`], [`IntoCheck`], and [`IntoCk`] from
//! `strck`, so other libraries only have to depend on this crate.
//!
//! # Feature flags
//! * `rust`: Provide the [`rust`] module, containing an [`Invariant`] and type
//! aliases to [`Ck`] and [`Check`] for Rust identifiers. Disabled by default.
//!
//! [`Invariant`]: strck::Invariant
//! [`RustIdent`]: rust::RustIdent
//! [`Ck`]: strck::Ck
//! [`Check`]: strck::Check

pub mod unicode;

#[doc(no_inline)]
pub use unicode::{Ident, IdentBuf};

#[cfg(feature = "rust")]
pub mod rust;

#[doc(no_inline)]
pub use strck::{Check, Ck, IntoCheck, IntoCk, Invariant};
