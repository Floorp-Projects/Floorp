// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(not(feature = "std"), no_std)]
#![cfg_attr(feature = "exact-size-is-empty", feature(exact_size_is_empty))]
#![cfg_attr(feature = "trusted-len", feature(trusted_len))]
#![warn(
    bad_style,
    missing_debug_implementations,
    missing_docs,
    unconditional_recursion
)]
#![deny(unsafe_code)]

//! # UNIC — Unicode Character Tools — Character Range
//!
//! A simple way to control iteration over a range of characters.
//!
//! # Examples
//!
//! ```
//! #[macro_use] extern crate unic_char_range;
//!
//! # fn main() {
//! for character in chars!('a'..='z') {
//!     // character is each character in the lowercase english alphabet in order
//! }
//!
//! for character in chars!(..) {
//!     // character is every valid char from lowest codepoint to highest
//! }
//! # }
//! ```
//!
//! # Features
//!
//! None of these features are included by default; they rely on unstable Rust feature gates.
//!
//! - `unstable`: enables all features
//! - `exact-size-is-empty`: provide a specific impl of [`ExactSizeIterator::is_empty`][is_empty]
//! - `trusted-len`: impl the [`TrustedLen`] contract
//!
//! [is_empty]: https://doc.rust-lang.org/std/iter/trait.ExactSizeIterator.html#method.is_empty
//! [`FusedIterator`]: https://doc.rust-lang.org/std/iter/trait.FusedIterator.html
//! [`TrustedLen`]: https://doc.rust-lang.org/std/iter/trait.TrustedLen.html
//!

mod pkg_info;
pub use crate::pkg_info::{PKG_DESCRIPTION, PKG_NAME, PKG_VERSION};

mod iter;
pub use crate::iter::CharIter;

mod range;
pub use crate::range::CharRange;

#[macro_use]
mod macros;

mod step;

mod iter_fused;

#[cfg(feature = "trusted-len")]
mod iter_trusted_len;

#[cfg(feature = "rayon")]
mod par_iter;
