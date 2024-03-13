/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Parse/serialize and resolve a single color component.

/// A single color component.
#[derive(Clone, MallocSizeOf, PartialEq, ToShmem)]
pub enum ColorComponent<ValueType> {
    /// The "none" keyword.
    None,
    /// A absolute value.
    Value(ValueType),
}

impl<ValueType> ColorComponent<ValueType> {
    /// Return true if the component is "none".
    #[inline]
    pub fn is_none(&self) -> bool {
        matches!(self, Self::None)
    }

    /// If the component contains a value, map it to another value.
    pub fn map_value<OutType>(
        self,
        f: impl FnOnce(ValueType) -> OutType,
    ) -> ColorComponent<OutType> {
        match self {
            Self::None => ColorComponent::None,
            Self::Value(value) => ColorComponent::Value(f(value)),
        }
    }
    /// Return the component as its value.
    pub fn into_value(self) -> ValueType {
        match self {
            Self::None => panic!("value not available when component is None"),
            Self::Value(value) => value,
        }
    }

    /// Return the component as its value or a default value.
    pub fn into_value_or(self, default: ValueType) -> ValueType {
        match self {
            Self::None => default,
            Self::Value(value) => value,
        }
    }
}
