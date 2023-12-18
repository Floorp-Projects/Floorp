/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Structs used for property declarations.

use super::{LonghandId, NonCustomPropertyId, PropertyFlags, PropertyId, ShorthandId};
use crate::custom_properties::Name;
#[cfg(feature = "gecko")]
use crate::gecko_bindings::structs::{nsCSSPropertyID, AnimatedPropertyID, RefPtr};
use crate::logical_geometry::WritingMode;
use crate::values::serialize_atom_name;
#[cfg(feature = "gecko")]
use crate::Atom;
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

    /// Returns whether this property is animatable.
    #[inline]
    pub fn is_animatable(&self) -> bool {
        match self {
            Self::Longhand(id) => id.is_animatable(),
            Self::Custom(_) => true,
        }
    }

    /// Returns the corresponding PropertyDeclarationId.
    #[inline]
    pub fn as_borrowed(&self) -> PropertyDeclarationId {
        match self {
            Self::Longhand(id) => PropertyDeclarationId::Longhand(*id),
            Self::Custom(name) => PropertyDeclarationId::Custom(name),
        }
    }

    /// Returns whether this property is transitionable.
    #[inline]
    pub fn is_transitionable(&self) -> bool {
        match self {
            Self::Longhand(longhand) => NonCustomPropertyId::from(*longhand).is_transitionable(),
            // TODO(bug 1846516): Implement this for custom properties.
            Self::Custom(_) => false,
        }
    }

    /// Convert an `AnimatedPropertyID` into an `OwnedPropertyDeclarationId`.
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn from_gecko_animated_property_id(property: &AnimatedPropertyID) -> Result<Self, ()> {
        if property.mID == nsCSSPropertyID::eCSSPropertyExtra_variable {
            if property.mCustomName.mRawPtr.is_null() {
                Err(())
            } else {
                Ok(Self::Custom(unsafe {
                    Atom::from_raw(property.mCustomName.mRawPtr)
                }))
            }
        } else if let Ok(longhand) = LonghandId::from_nscsspropertyid(property.mID) {
            Ok(Self::Longhand(longhand))
        } else {
            Err(())
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
            PropertyDeclarationId::Custom(name) => {
                dest.write_str("--")?;
                serialize_atom_name(name, dest)
            },
        }
    }
}

impl<'a> PropertyDeclarationId<'a> {
    /// Returns PropertyFlags for given property.
    #[inline(always)]
    pub fn flags(&self) -> PropertyFlags {
        match self {
            Self::Longhand(id) => id.flags(),
            Self::Custom(_) => PropertyFlags::empty(),
        }
    }

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
            PropertyDeclarationId::Custom(_) => true,
        }
    }

    /// Returns whether this property is animatable in a discrete way.
    #[inline]
    pub fn is_discrete_animatable(&self) -> bool {
        match self {
            Self::Longhand(longhand) => longhand.is_discrete_animatable(),
            // TODO(bug 1846516): Refine this?
            Self::Custom(_) => true,
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

    /// Convert a `PropertyDeclarationId` into an `AnimatedPropertyID`
    #[cfg(feature = "gecko")]
    #[inline]
    pub fn to_gecko_animated_property_id(&self) -> AnimatedPropertyID {
        match self {
            Self::Longhand(id) => AnimatedPropertyID {
                mID: id.to_nscsspropertyid(),
                mCustomName: RefPtr::null(),
            },
            Self::Custom(name) => {
                let mut property_id = AnimatedPropertyID {
                    mID: nsCSSPropertyID::eCSSPropertyExtra_variable,
                    mCustomName: RefPtr::null(),
                };
                property_id.mCustomName.mRawPtr = (*name).clone().into_addrefed();
                property_id
            },
        }
    }
}
