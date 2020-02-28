// Copyright 2017 The UNIC Project Developers.
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

//! # UNIC — Unicode Character Tools — Character Property
//!
//! A component of [`unic`: Unicode and Internationalization Crates for Rust](/unic/).
//!
//! Character Property taxonomy, contracts and build macros.
//!
//! ## References
//!
//! * [Unicode UTR #23: The Unicode Character Property Model](http://unicode.org/reports/tr23/).
//!
//! * [Unicode UAX #44: Unicode Character Database](http://unicode.org/reports/tr44/).
//!
//! * [PropertyAliases.txt](https://www.unicode.org/Public/UCD/latest/ucd/PropertyAliases.txt).

#[macro_use]
extern crate unic_char_range;

mod pkg_info;
pub use crate::pkg_info::{PKG_DESCRIPTION, PKG_NAME, PKG_VERSION};

mod property;
pub use self::property::{CharProperty, PartialCharProperty, TotalCharProperty};

mod range_types;
pub use crate::range_types::{
    BinaryCharProperty,
    CustomCharProperty,
    EnumeratedCharProperty,
    NumericCharProperty,
    NumericCharPropertyValue,
};

mod macros;

// pub because is used in macros, called from macro call-site.
pub mod tables;

// Used in macros
#[doc(hidden)]
pub use core::{fmt as __fmt, str as __str};
