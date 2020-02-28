// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Taxonomy and contracts for Character Property types.

use core::fmt::Debug;
use core::hash::Hash;

/// A Character Property, defined for some or all Unicode characters.
pub trait CharProperty: PartialCharProperty + Debug + Eq + Hash {
    /// The *abbreviated name* of the property.
    fn prop_abbr_name() -> &'static str;

    /// The *long name* of the property.
    fn prop_long_name() -> &'static str;

    /// The *human-readable* name of the property.
    fn prop_human_name() -> &'static str;
}

/// A Character Property defined for some characters.
///
/// Examples: `Decomposition_Type`, `Numeric_Type`
pub trait PartialCharProperty: Copy {
    /// The property value for the character, or None.
    fn of(ch: char) -> Option<Self>;
}

/// A Character Property defined on all characters.
///
/// Examples: `Age`, `Name`, `General_Category`, `Bidi_Class`
pub trait TotalCharProperty: PartialCharProperty + Default {
    /// The property value for the character.
    fn of(ch: char) -> Self;
}

impl<T: TotalCharProperty> PartialCharProperty for T {
    fn of(ch: char) -> Option<Self> {
        Some(<Self as TotalCharProperty>::of(ch))
    }
}
