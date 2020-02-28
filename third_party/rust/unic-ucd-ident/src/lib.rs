// Copyright 2017-2019 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![no_std]
#![warn(
    bad_style,
    missing_debug_implementations,
    missing_docs,
    unconditional_recursion
)]
#![forbid(unsafe_code)]

//! # UNIC — UCD — Identifier Properties
//!
//! A component of [`unic`: Unicode and Internationalization Crates for Rust](/unic/).
//!
//! Accessor to the UCD properties used widely by [UAX31 Unicode Identifier and Pattern Syntax].
//!
//! # Features
//!
//! - `xid` (default): the `XID_Start` and `XID_Continue` properties.
//!
//! - `id` (optional): the `ID_Start` and `ID_Continue` properties.
//!   NOTE: in most cases, you should prefer using the `XID` properties
//!   because they are consistent under NFKC normalization.
//!
//! - `pattern` (optional): the `Pattern_Syntax` and `Pattern_White_Space` properties.
//!
//! [UAX31 Unicode Identifier and Pattern Syntax]: <https://www.unicode.org/reports/tr31/>

#[macro_use]
extern crate unic_char_property;
#[macro_use]
extern crate unic_char_range;

mod pkg_info;
pub use crate::pkg_info::{PKG_DESCRIPTION, PKG_NAME, PKG_VERSION};

#[cfg(feature = "xid")]
mod xid;
#[cfg(feature = "xid")]
pub use crate::xid::{is_xid_continue, is_xid_start, XidContinue, XidStart};

#[cfg(feature = "id")]
mod id;
#[cfg(feature = "id")]
pub use crate::id::{is_id_continue, is_id_start, IdContinue, IdStart};

#[cfg(feature = "pattern")]
mod pattern;
#[cfg(feature = "pattern")]
pub use crate::pattern::{
    is_pattern_syntax,
    is_pattern_whitespace,
    PatternSyntax,
    PatternWhitespace,
};

use unic_ucd_version::UnicodeVersion;

/// The [Unicode version](https://www.unicode.org/versions/) of data
pub const UNICODE_VERSION: UnicodeVersion = include!("../tables/unicode_version.rsv");
