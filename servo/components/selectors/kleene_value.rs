/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Kleen logic: https://en.wikipedia.org/wiki/Three-valued_logic#Kleene_and_Priest_logics

/// A "trilean" value based on Kleen logic.
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum KleeneValue {
    /// False
    False = 0,
    /// True
    True = 1,
    /// Either true or false, but we’re not sure which yet.
    Unknown,
}

impl From<bool> for KleeneValue {
    fn from(b: bool) -> Self {
        if b {
            Self::True
        } else {
            Self::False
        }
    }
}

impl KleeneValue {
    /// Turns this Kleene value to a bool, taking the unknown value as an
    /// argument.
    pub fn to_bool(self, unknown: bool) -> bool {
        match self {
            Self::True => true,
            Self::False => false,
            Self::Unknown => unknown,
        }
    }
}

impl std::ops::Not for KleeneValue {
    type Output = Self;

    fn not(self) -> Self {
        match self {
            Self::True => Self::False,
            Self::False => Self::True,
            Self::Unknown => Self::Unknown,
        }
    }
}

// Implements the logical and operation.
impl std::ops::BitAnd for KleeneValue {
    type Output = Self;

    fn bitand(self, other: Self) -> Self {
        if self == Self::False || other == Self::False {
            return Self::False;
        }
        if self == Self::Unknown || other == Self::Unknown {
            return Self::Unknown;
        }
        Self::True
    }
}

// Implements the logical or operation.
impl std::ops::BitOr for KleeneValue {
    type Output = Self;

    fn bitor(self, other: Self) -> Self {
        if self == Self::True || other == Self::True {
            return Self::True;
        }
        if self == Self::Unknown || other == Self::Unknown {
            return Self::Unknown;
        }
        Self::False
    }
}

impl std::ops::BitOrAssign for KleeneValue {
    fn bitor_assign(&mut self, other: Self) {
        *self = *self | other;
    }
}

impl std::ops::BitAndAssign for KleeneValue {
    fn bitand_assign(&mut self, other: Self) {
        *self = *self & other;
    }
}
