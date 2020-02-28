// Copyright 2017 The UNIC Project Developers.
//
// See the COPYRIGHT file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Character Property Range types.
//!
//! NOTE: At the moment, it is not possible to define a marker for all character property range
//! types and enforce their implementation from `CharProperty`.  We need to fix this whenever the
//! compiler becomes able to do to so.

use super::property::CharProperty;

// == Enumerated/Catalog Types ==

/// A Character Property with enumerated values.
///
/// This is similar to types *Enumeration* and *Catalog*, as defined in UAX#44.
///
/// Usage Note: If the property is of type *Catalog*, it's recommended to (in some way) mark the
/// type as *non-exhaustive*, so that adding new variants to the `enum` type won't result in API
/// breakage.
pub trait EnumeratedCharProperty: Sized + CharProperty {
    /// Exhaustive list of all property values.
    fn all_values() -> &'static [Self];

    /// The *abbreviated name* of the property value.
    fn abbr_name(&self) -> &'static str;

    /// The *long name* of the property value.
    fn long_name(&self) -> &'static str;

    /// The *human-readable name* of the property value.
    fn human_name(&self) -> &'static str;
}

// == Binary Types ==

/// A Character Property with binary values.
///
/// Examples: `Alphabetic`, `Bidi_Mirrored`, `White_Space`
pub trait BinaryCharProperty: CharProperty {
    /// The boolean value of the property value.
    fn as_bool(&self) -> bool;

    /// The *abbreviated name* of the property value.
    fn abbr_name(&self) -> &'static str {
        if self.as_bool() {
            "Y"
        } else {
            "N"
        }
    }

    /// The *long name* of the property value.
    fn long_name(&self) -> &'static str {
        if self.as_bool() {
            "Yes"
        } else {
            "No"
        }
    }

    /// The *human-readable name* of the property value.
    fn human_name(&self) -> &'static str {
        if self.as_bool() {
            "Yes"
        } else {
            "No"
        }
    }
}

// == Numeric Types ==

/// Marker for numeric types accepted by `NumericCharProperty`.
pub trait NumericCharPropertyValue {}

impl NumericCharPropertyValue for u8 {}

/// A Character Property with numeric values.
///
/// Examples: `Numeric_Value`, `Canonical_Combining_Class`
pub trait NumericCharProperty<NumericValue: NumericCharPropertyValue>: CharProperty {
    /// The numeric value for the property value.
    fn number(&self) -> NumericValue;
}

// == Custom Types ==

/// A Character Property with custom values.
///
/// Custom values means any non-enumerated, non-numeric value.
///
/// Examples: `Age` property that returns a `UnicodeVersion` value.
pub trait CustomCharProperty<Value>: CharProperty {
    /// The actual (inner) value for the property value.
    fn actual(&self) -> Value;
}
