// Copyright 2013-2014 The Rust Project Developers.
// Copyright 2018 The Uuid Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Adapters for various formats for [`Uuid`]s
//!
//! [`Uuid`]: ../struct.Uuid.html

/// The length of a Hyphenated [`Uuid`] string.
///
/// [`Uuid`]: ../struct.Uuid.html
pub const UUID_HYPHENATED_LENGTH: usize = 36;

/// The length of a Simple [`Uuid`] string.
///
/// [`Uuid`]: ../struct.Uuid.html
pub const UUID_SIMPLE_LENGTH: usize = 32;

/// The length of a Urn [`Uuid`] string.
///
/// [`Uuid`]: ../struct.Uuid.html
// TODO: remove #[allow(dead_code)] lint
// BODY: This only exists to allow compiling the code currently,
#[allow(dead_code)]
pub const UUID_URN_LENGTH: usize = 45;
