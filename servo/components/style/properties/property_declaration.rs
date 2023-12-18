/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Structs used for property declarations.

use super::{LonghandId, PropertyId, ShorthandId};
use crate::custom_properties::Name;
#[cfg(feature = "gecko")]
use crate::gecko_bindings::structs::nsCSSPropertyID;
use crate::logical_geometry::WritingMode;
use crate::values::serialize_atom_name;
use std::{
    borrow::Cow,
    fmt::{self, Write},
};
use style_traits::{CssWriter, ToCss};

/// A PropertyDeclarationId without references, for use as a hash map key.
#[derive(Clone, Debug, PartialEq, Eq, Hash)]
pub enum OwnedPropertyDeclarationId {
    /// A longhand.
    Longhand(LonghandId),
    /// A custom property declaration.
    Custom(Name),
}

impl OwnedPropertyDeclarationId {
    /// Return whether this property is logical.
    #[inline]
    pub fn is_logical(&self) -> bool {
        self.as_borrowed().is_logical()
    }

    /// Returns the corresponding PropertyDeclarationId.
    #[inline]
    pub fn as_borrowed(&self) -> PropertyDeclarationId {
        match self {
            OwnedPropertyDeclarationId::Longhand(id) => PropertyDeclarationId::Longhand(*id),
            OwnedPropertyDeclarationId::Custom(name) => PropertyDeclarationId::Custom(name),
        }
    }
}

/// An identifier for a given property declaration, which can be either a
/// longhand or a custom property.
#[derive(Clone, Copy, Debug, PartialEq)]
#[cfg_attr(feature = "servo", derive(MallocSizeOf))]
pub enum PropertyDeclarationId<'a> {
    /// A longhand.
    Longhand(LonghandId),
    /// A custom property declaration.
    Custom(&'a Name),
}

impl<'a> ToCss for PropertyDeclarationId<'a> {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        match *self {
            PropertyDeclarationId::Longhand(id) => dest.write_str(id.name()),
            PropertyDeclarationId::Custom(ref name) => {
                dest.write_str("--")?;
                serialize_atom_name(name, dest)
            },
        }
    }
}

impl<'a> PropertyDeclarationId<'a> {
    /// Convert to an OwnedPropertyDeclarationId.
    pub fn to_owned(&self) -> OwnedPropertyDeclarationId {
        match self {
            PropertyDeclarationId::Longhand(id) => OwnedPropertyDeclarationId::Longhand(*id),
            PropertyDeclarationId::Custom(name) => {
                OwnedPropertyDeclarationId::Custom((*name).clone())
            },
        }
    }

    /// Whether a given declaration id is either the same as `other`, or a
    /// longhand of it.
    pub fn is_or_is_longhand_of(&self, other: &PropertyId) -> bool {
        match *self {
            PropertyDeclarationId::Longhand(id) => match *other {
                PropertyId::NonCustom(non_custom_id) => id.is_or_is_longhand_of(non_custom_id),
                PropertyId::Custom(_) => false,
            },
            PropertyDeclarationId::Custom(name) => {
                matches!(*other, PropertyId::Custom(ref other_name) if name == other_name)
            },
        }
    }

    /// Whether a given declaration id is a longhand belonging to this
    /// shorthand.
    pub fn is_longhand_of(&self, shorthand: ShorthandId) -> bool {
        match *self {
            PropertyDeclarationId::Longhand(ref id) => id.is_longhand_of(shorthand),
            _ => false,
        }
    }

    /// Returns the name of the property without CSS escaping.
    pub fn name(&self) -> Cow<'static, str> {
        match *self {
            PropertyDeclarationId::Longhand(id) => id.name().into(),
            PropertyDeclarationId::Custom(name) => {
                let mut s = String::new();
                write!(&mut s, "--{}", name).unwrap();
                s.into()
            },
        }
    }

    /// Returns longhand id if it is, None otherwise.
    #[inline]
    pub fn as_longhand(&self) -> Option<LonghandId> {
        match *self {
            PropertyDeclarationId::Longhand(id) => Some(id),
            _ => None,
        }
    }

    /// Return whether this property is logical.
    #[inline]
    pub fn is_logical(&self) -> bool {
        match self {
            PropertyDeclarationId::Longhand(id) => id.is_logical(),
            PropertyDeclarationId::Custom(_) => false,
        }
    }

    /// If this is a logical property, return the corresponding physical one in
    /// the given writing mode.
    ///
    /// Otherwise, return unchanged.
    #[inline]
    pub fn to_physical(&self, wm: WritingMode) -> Self {
        match self {
            PropertyDeclarationId::Longhand(id) => {
                PropertyDeclarationId::Longhand(id.to_physical(wm))
            },
            PropertyDeclarationId::Custom(_) => self.clone(),
        }
    }

    /// Returns whether this property is animatable.
    #[inline]
    pub fn is_animatable(&self) -> bool {
        match self {
            PropertyDeclarationId::Longhand(id) => id.is_animatable(),
            // TODO(bug 1846516): This should return true.
            PropertyDeclarationId::Custom(_) => false,
        }
    }

    /// Converts from a to an adequate nsCSSPropertyID, returning
    /// eCSSPropertyExtra_variable for custom properties.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn to_nscsspropertyid(self) -> nsCSSPropertyID {
        match self {
            PropertyDeclarationId::Longhand(id) => id.to_nscsspropertyid(),
            PropertyDeclarationId::Custom(_) => nsCSSPropertyID::eCSSPropertyExtra_variable,
        }
    }
}
