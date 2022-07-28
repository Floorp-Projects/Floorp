//
// Copyright 2019 Red Hat, Inc.
//
// Author: Nathaniel McCallum <npmccallum@redhat.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//! # Welcome to FlagSet!
//!
//! FlagSet is a new, ergonomic approach to handling flags that combines the
//! best of existing crates like `bitflags` and `enumflags` without their
//! downsides.
//!
//! ## Existing Implementations
//!
//! The `bitflags` crate has long been part of the Rust ecosystem.
//! Unfortunately, it doesn't feel like natural Rust. The `bitflags` crate
//! uses a wierd struct format to define flags. Flags themselves are just
//! integers constants, so there is little type-safety involved. But it doesn't
//! have any dependencies. It also allows you to define implied flags (otherwise
//! known as overlapping flags).
//!
//! The `enumflags` crate tried to improve on `bitflags` by using enumerations
//! to define flags. This was a big improvement to the natural feel of the code.
//! Unfortunately, there are some design flaws. To generate the flags,
//! procedural macros were used. This implied two separate crates plus
//! additional dependencies. Further, `enumflags` specifies the size of the
//! flags using a `repr($size)` attribute. Unfortunately, this attribute
//! cannot resolve type aliases, such as `c_int`. This makes `enumflags` a
//! poor fit for FFI, which is the most important place for a flags library.
//! The `enumflags` crate also disallows overlapping flags and is not
//! maintained.
//!
//! FlagSet improves on both of these by adopting the `enumflags` natural feel
//! and the `bitflags` mode of flag generation; as well as additional API usage
//! niceties. FlagSet has no dependencies and is extensively documented and
//! tested. It also tries very hard to prevent you from making mistakes by
//! avoiding external usage of the integer types. FlagSet is also a zero-cost
//! abstraction: all functions are inlineable and should reduce to the core
//! integer operations. FlagSet also does not depend on stdlib, so it can be
//! used in `no_std` libraries and applications.
//!
//! ## Defining Flags
//!
//! Flags are defined using the `flags!` macro:
//!
//! ```
//! use flagset::{FlagSet, flags};
//! use std::os::raw::c_int;
//!
//! flags! {
//!     enum FlagsA: u8 {
//!         Foo,
//!         Bar,
//!         Baz,
//!     }
//!
//!     enum FlagsB: c_int {
//!         Foo,
//!         Bar,
//!         Baz,
//!     }
//! }
//! ```
//!
//! Notice that a flag definition looks just like a regular enumeration, with
//! the addition of the field-size type. The field-size type is required and
//! can be either a type or a type alias. Both examples are given above.
//!
//! Also note that the field-size type specifies the size of the corresponding
//! `FlagSet` type, not size of the enumeration itself. To specify the size of
//! the enumeration, use the `repr($size)` attribute as specified below.
//!
//! ## Flag Values
//!
//! Flags often need values assigned to them. This can be done implicitly,
//! where the value depends on the order of the flags:
//!
//! ```
//! use flagset::{FlagSet, flags};
//!
//! flags! {
//!     enum Flags: u16 {
//!         Foo, // Implicit Value: 0b0001
//!         Bar, // Implicit Value: 0b0010
//!         Baz, // Implicit Value: 0b0100
//!     }
//! }
//! ```
//!
//! Alternatively, flag values can be defined explicitly, by specifying any
//! `const` expression:
//!
//! ```
//! use flagset::{FlagSet, flags};
//!
//! flags! {
//!     enum Flags: u16 {
//!         Foo = 0x01,   // Explicit Value: 0b0001
//!         Bar = 2,      // Explicit Value: 0b0010
//!         Baz = 0b0100, // Explicit Value: 0b0100
//!     }
//! }
//! ```
//!
//! Flags can also overlap or "imply" other flags:
//!
//! ```
//! use flagset::{FlagSet, flags};
//!
//! flags! {
//!     enum Flags: u16 {
//!         Foo = 0b0001,
//!         Bar = 0b0010,
//!         Baz = 0b0110, // Implies Bar
//!         All = (Flags::Foo | Flags::Bar | Flags::Baz).bits(),
//!     }
//! }
//! ```
//!
//! ## Specifying Attributes
//!
//! Attributes can be used on the enumeration itself or any of the values:
//!
//! ```
//! use flagset::{FlagSet, flags};
//!
//! flags! {
//!     #[derive(PartialOrd, Ord)]
//!     enum Flags: u8 {
//!         Foo,
//!         #[deprecated]
//!         Bar,
//!         Baz,
//!     }
//! }
//! ```
//!
//! ## Collections of Flags
//!
//! A collection of flags is a `FlagSet<T>`. If you are storing the flags in
//! memory, the raw `FlagSet<T>` type should be used. However, if you want to
//! receive flags as an input to a function, you should use
//! `impl Into<FlagSet<T>>`. This allows for very ergonomic APIs:
//!
//! ```
//! use flagset::{FlagSet, flags};
//!
//! flags! {
//!     enum Flags: u8 {
//!         Foo,
//!         Bar,
//!         Baz,
//!     }
//! }
//!
//! struct Container(FlagSet<Flags>);
//!
//! impl Container {
//!     fn new(flags: impl Into<FlagSet<Flags>>) -> Container {
//!         Container(flags.into())
//!     }
//! }
//!
//! assert_eq!(Container::new(Flags::Foo | Flags::Bar).0.bits(), 0b011);
//! assert_eq!(Container::new(Flags::Foo).0.bits(), 0b001);
//! assert_eq!(Container::new(None).0.bits(), 0b000);
//! ```
//!
//! ## Operations
//!
//! Operations can be performed on a `FlagSet<F>` or on individual flags:
//!
//! | Operator | Assignment Operator | Meaning                |
//! |----------|---------------------|------------------------|
//! | \|       | \|=                 | Union                  |
//! | &        | &=                  | Intersection           |
//! | ^        | ^=                  | Toggle specified flags |
//! | -        | -=                  | Difference             |
//! | %        | %=                  | Symmetric difference   |
//! | !        |                     | Toggle all flags       |
//!
#![cfg_attr(
    feature = "serde",
    doc = r#"

## Optional Serde support

[Serde] support can be enabled with the 'serde' feature flag. You can then serialize and
deserialize `FlagSet<T>` to and from any of the [supported formats]:

 ```
 use flagset::{FlagSet, flags};

 flags! {
     enum Flags: u8 {
         Foo,
         Bar,
     }
 }

 let flagset = Flags::Foo | Flags::Bar;
 let json = serde_json::to_string(&flagset).unwrap();
 let flagset: FlagSet<Flags> = serde_json::from_str(&json).unwrap();
 assert_eq!(flagset.bits(), 0b011);
 ```

For serialization and deserialization of flags enum itself, you can use the [`serde_repr`] crate
(or implement `serde::ser::Serialize` and `serde:de::Deserialize` manually), combined with the
appropriate `repr` attribute:

 ```
 use flagset::{FlagSet, flags};
 use serde_repr::{Serialize_repr, Deserialize_repr};

 flags! {
    #[repr(u8)]
    #[derive(Deserialize_repr, Serialize_repr)]
    enum Flags: u8 {
         Foo,
         Bar,
    }
 }

 let json = serde_json::to_string(&Flags::Foo).unwrap();
 let flag: Flags = serde_json::from_str(&json).unwrap();
 assert_eq!(flag, Flags::Foo);
 ```

[Serde]: https://serde.rs/
[supported formats]: https://serde.rs/#data-formats
[`serde_repr`]: https://crates.io/crates/serde_repr
"#
)]
#![allow(unknown_lints)]
#![warn(clippy::all)]
#![no_std]

use core::fmt::{Debug, Formatter, Result};
use core::ops::*;

/// Error type returned when creating a new flagset from bits is invalid or undefined.
/// ```
/// use flagset::{FlagSet, flags};
///
/// flags! {
///     pub enum Flag: u16 {
///         Foo = 0b0001,
///         Bar = 0b0010,
///         Baz = 0b0100,
///         Qux = 0b1010, // Implies Bar
///     }
/// }
///
/// assert_eq!(FlagSet::<Flag>::new(0b01101), Err(flagset::InvalidBits)); // Invalid
/// assert_eq!(FlagSet::<Flag>::new(0b10101), Err(flagset::InvalidBits)); // Unknown
/// ```
#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct InvalidBits;

impl core::fmt::Display for InvalidBits {
    #[inline]
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        write!(f, "invalid bits")
    }
}

#[doc(hidden)]
pub trait Flags:
    Copy
    + Clone
    + Debug
    + PartialEq
    + Eq
    + BitAnd<Self, Output = FlagSet<Self>>
    + BitOr<Self, Output = FlagSet<Self>>
    + BitXor<Self, Output = FlagSet<Self>>
    + Sub<Self, Output = FlagSet<Self>>
    + Rem<Self, Output = FlagSet<Self>>
    + Not<Output = FlagSet<Self>>
    + Into<FlagSet<Self>>
    + 'static
{
    type Type: Copy
        + Clone
        + Debug
        + PartialEq
        + Eq
        + Default
        + BitAnd<Self::Type, Output = Self::Type>
        + BitAndAssign<Self::Type>
        + BitOr<Self::Type, Output = Self::Type>
        + BitOrAssign<Self::Type>
        + BitXor<Self::Type, Output = Self::Type>
        + BitXorAssign<Self::Type>
        + Not<Output = Self::Type>;

    /// A slice containing all the possible flag values.
    const LIST: &'static [Self];

    /// Creates an empty `FlagSet` of this type
    #[inline]
    fn none() -> FlagSet<Self> {
        FlagSet::default()
    }
}

#[derive(Copy, Clone, Eq)]
pub struct FlagSet<F: Flags>(F::Type);

#[doc(hidden)]
#[derive(Copy, Clone)]
pub struct Iter<F: Flags>(FlagSet<F>, usize);

impl<F: Flags> Iterator for Iter<F> {
    type Item = F;

    #[inline]
    fn next(&mut self) -> Option<Self::Item> {
        while self.1 < F::LIST.len() {
            let next = F::LIST[self.1];
            self.1 += 1;

            if self.0.contains(next) {
                return Some(next);
            }
        }

        None
    }
}

impl<F: Flags> IntoIterator for FlagSet<F> {
    type Item = F;
    type IntoIter = Iter<F>;

    /// Iterate over the flags in the set.
    ///
    /// **NOTE**: The order in which the flags are iterated is undefined.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     enum Flag: u8 {
    ///         Foo = 0b001,
    ///         Bar = 0b010,
    ///         Baz = 0b100
    ///     }
    /// }
    ///
    /// let set = Flag::Foo | Flag::Bar;
    /// let mut iter = set.into_iter();
    /// assert_eq!(iter.next(), Some(Flag::Foo));
    /// assert_eq!(iter.next(), Some(Flag::Bar));
    /// assert_eq!(iter.next(), None);
    /// ```
    #[inline]
    fn into_iter(self) -> Self::IntoIter {
        Iter(self, 0)
    }
}

impl<F: Flags> Debug for FlagSet<F> {
    #[inline]
    fn fmt(&self, f: &mut Formatter) -> Result {
        write!(f, "FlagSet(")?;
        for (i, flag) in self.into_iter().enumerate() {
            write!(f, "{}{:?}", if i > 0 { " | " } else { "" }, flag)?;
        }
        write!(f, ")")
    }
}

impl<F: Flags, R: Copy + Into<FlagSet<F>>> PartialEq<R> for FlagSet<F> {
    #[inline]
    fn eq(&self, rhs: &R) -> bool {
        self.0 == (*rhs).into().0
    }
}

impl<F: Flags> AsRef<F::Type> for FlagSet<F> {
    #[inline]
    fn as_ref(&self) -> &F::Type {
        &self.0
    }
}

impl<F: Flags> From<Option<FlagSet<F>>> for FlagSet<F> {
    /// Converts from `Option<FlagSet<F>>` to `FlagSet<F>`.
    ///
    /// Most notably, this allows for the use of `None` in many places to
    /// substitute for manually creating an empty `FlagSet<F>`. See below.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     enum Flag: u8 {
    ///         Foo = 0b001,
    ///         Bar = 0b010,
    ///         Baz = 0b100
    ///     }
    /// }
    ///
    /// fn convert(v: impl Into<FlagSet<Flag>>) -> u8 {
    ///     v.into().bits()
    /// }
    ///
    /// assert_eq!(convert(Flag::Foo | Flag::Bar), 0b011);
    /// assert_eq!(convert(Flag::Foo), 0b001);
    /// assert_eq!(convert(None), 0b000);
    /// ```
    #[inline]
    fn from(value: Option<FlagSet<F>>) -> FlagSet<F> {
        value.unwrap_or_default()
    }
}

impl<F: Flags> Default for FlagSet<F> {
    /// Creates a new, empty FlagSet.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     enum Flag: u8 {
    ///         Foo = 0b001,
    ///         Bar = 0b010,
    ///         Baz = 0b100
    ///     }
    /// }
    ///
    /// let set = FlagSet::<Flag>::default();
    /// assert!(set.is_empty());
    /// assert!(!set.is_full());
    /// assert!(!set.contains(Flag::Foo));
    /// assert!(!set.contains(Flag::Bar));
    /// assert!(!set.contains(Flag::Baz));
    /// ```
    #[inline]
    fn default() -> Self {
        FlagSet(F::Type::default())
    }
}

impl<F: Flags> Not for FlagSet<F> {
    type Output = Self;

    /// Calculates the complement of the current set.
    ///
    /// In common parlance, this returns the set of all possible flags that are
    /// not in the current set.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     #[derive(PartialOrd, Ord)]
    ///     enum Flag: u8 {
    ///         Foo = 1 << 0,
    ///         Bar = 1 << 1,
    ///         Baz = 1 << 2
    ///     }
    /// }
    ///
    /// let set = !FlagSet::from(Flag::Foo);
    /// assert!(!set.is_empty());
    /// assert!(!set.is_full());
    /// assert!(!set.contains(Flag::Foo));
    /// assert!(set.contains(Flag::Bar));
    /// assert!(set.contains(Flag::Baz));
    /// ```
    #[inline]
    fn not(self) -> Self {
        FlagSet(!self.0)
    }
}

impl<F: Flags, R: Into<FlagSet<F>>> BitAnd<R> for FlagSet<F> {
    type Output = Self;

    /// Calculates the intersection of the current set and the specified flags.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     #[derive(PartialOrd, Ord)]
    ///     pub enum Flag: u8 {
    ///         Foo = 0b001,
    ///         Bar = 0b010,
    ///         Baz = 0b100
    ///     }
    /// }
    ///
    /// let set0 = Flag::Foo | Flag::Bar;
    /// let set1 = Flag::Baz | Flag::Bar;
    /// assert_eq!(set0 & set1, Flag::Bar);
    /// assert_eq!(set0 & Flag::Foo, Flag::Foo);
    /// assert_eq!(set1 & Flag::Baz, Flag::Baz);
    /// ```
    #[inline]
    fn bitand(self, rhs: R) -> Self {
        FlagSet(self.0 & rhs.into().0)
    }
}

impl<F: Flags, R: Into<FlagSet<F>>> BitAndAssign<R> for FlagSet<F> {
    /// Assigns the intersection of the current set and the specified flags.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     enum Flag: u64 {
    ///         Foo = 0b001,
    ///         Bar = 0b010,
    ///         Baz = 0b100
    ///     }
    /// }
    ///
    /// let mut set0 = Flag::Foo | Flag::Bar;
    /// let mut set1 = Flag::Baz | Flag::Bar;
    ///
    /// set0 &= set1;
    /// assert_eq!(set0, Flag::Bar);
    ///
    /// set1 &= Flag::Baz;
    /// assert_eq!(set0, Flag::Bar);
    /// ```
    #[inline]
    fn bitand_assign(&mut self, rhs: R) {
        self.0 &= rhs.into().0
    }
}

impl<F: Flags, R: Into<FlagSet<F>>> BitOr<R> for FlagSet<F> {
    type Output = Self;

    /// Calculates the union of the current set with the specified flags.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     #[derive(PartialOrd, Ord)]
    ///     pub enum Flag: u8 {
    ///         Foo = 0b001,
    ///         Bar = 0b010,
    ///         Baz = 0b100
    ///     }
    /// }
    ///
    /// let set0 = Flag::Foo | Flag::Bar;
    /// let set1 = Flag::Baz | Flag::Bar;
    /// assert_eq!(set0 | set1, FlagSet::full());
    /// ```
    #[inline]
    fn bitor(self, rhs: R) -> Self {
        FlagSet(self.0 | rhs.into().0)
    }
}

impl<F: Flags, R: Into<FlagSet<F>>> BitOrAssign<R> for FlagSet<F> {
    /// Assigns the union of the current set with the specified flags.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     enum Flag: u64 {
    ///         Foo = 0b001,
    ///         Bar = 0b010,
    ///         Baz = 0b100
    ///     }
    /// }
    ///
    /// let mut set0 = Flag::Foo | Flag::Bar;
    /// let mut set1 = Flag::Bar | Flag::Baz;
    ///
    /// set0 |= set1;
    /// assert_eq!(set0, FlagSet::full());
    ///
    /// set1 |= Flag::Baz;
    /// assert_eq!(set1, Flag::Bar | Flag::Baz);
    /// ```
    #[inline]
    fn bitor_assign(&mut self, rhs: R) {
        self.0 |= rhs.into().0
    }
}

impl<F: Flags, R: Into<FlagSet<F>>> BitXor<R> for FlagSet<F> {
    type Output = Self;

    /// Calculates the current set with the specified flags toggled.
    ///
    /// This is commonly known as toggling the presence
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     enum Flag: u32 {
    ///         Foo = 0b001,
    ///         Bar = 0b010,
    ///         Baz = 0b100
    ///     }
    /// }
    ///
    /// let set0 = Flag::Foo | Flag::Bar;
    /// let set1 = Flag::Baz | Flag::Bar;
    /// assert_eq!(set0 ^ set1, Flag::Foo | Flag::Baz);
    /// assert_eq!(set0 ^ Flag::Foo, Flag::Bar);
    /// ```
    #[inline]
    fn bitxor(self, rhs: R) -> Self {
        FlagSet(self.0 ^ rhs.into().0)
    }
}

impl<F: Flags, R: Into<FlagSet<F>>> BitXorAssign<R> for FlagSet<F> {
    /// Assigns the current set with the specified flags toggled.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     enum Flag: u16 {
    ///         Foo = 0b001,
    ///         Bar = 0b010,
    ///         Baz = 0b100
    ///     }
    /// }
    ///
    /// let mut set0 = Flag::Foo | Flag::Bar;
    /// let mut set1 = Flag::Baz | Flag::Bar;
    ///
    /// set0 ^= set1;
    /// assert_eq!(set0, Flag::Foo | Flag::Baz);
    ///
    /// set1 ^= Flag::Baz;
    /// assert_eq!(set1, Flag::Bar);
    /// ```
    #[inline]
    fn bitxor_assign(&mut self, rhs: R) {
        self.0 ^= rhs.into().0
    }
}

impl<F: Flags, R: Into<FlagSet<F>>> Sub<R> for FlagSet<F> {
    type Output = Self;

    /// Calculates set difference (the current set without the specified flags).
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let set0 = Flag::Foo | Flag::Bar;
    /// let set1 = Flag::Baz | Flag::Bar;
    /// assert_eq!(set0 - set1, Flag::Foo);
    /// ```
    #[inline]
    fn sub(self, rhs: R) -> Self {
        self & !rhs.into()
    }
}

impl<F: Flags, R: Into<FlagSet<F>>> SubAssign<R> for FlagSet<F> {
    /// Assigns set difference (the current set without the specified flags).
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let mut set0 = Flag::Foo | Flag::Bar;
    /// set0 -= Flag::Baz | Flag::Bar;
    /// assert_eq!(set0, Flag::Foo);
    /// ```
    #[inline]
    fn sub_assign(&mut self, rhs: R) {
        *self &= !rhs.into();
    }
}

impl<F: Flags, R: Into<FlagSet<F>>> Rem<R> for FlagSet<F> {
    type Output = Self;

    /// Calculates the symmetric difference between two sets.
    ///
    /// The symmetric difference between two sets is the set of all flags
    /// that appear in one set or the other, but not both.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let set0 = Flag::Foo | Flag::Bar;
    /// let set1 = Flag::Baz | Flag::Bar;
    /// assert_eq!(set0 % set1, Flag::Foo | Flag::Baz);
    /// ```
    #[inline]
    fn rem(self, rhs: R) -> Self {
        let rhs = rhs.into();
        (self - rhs) | (rhs - self)
    }
}

impl<F: Flags, R: Into<FlagSet<F>>> RemAssign<R> for FlagSet<F> {
    /// Assigns the symmetric difference between two sets.
    ///
    /// The symmetric difference between two sets is the set of all flags
    /// that appear in one set or the other, but not both.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let mut set0 = Flag::Foo | Flag::Bar;
    /// let set1 = Flag::Baz | Flag::Bar;
    /// set0 %= set1;
    /// assert_eq!(set0, Flag::Foo | Flag::Baz);
    /// ```
    #[inline]
    fn rem_assign(&mut self, rhs: R) {
        *self = *self % rhs
    }
}

impl<F: Flags> FlagSet<F> {
    /// Creates a new set from bits; returning `Err(InvalidBits)` on invalid/unknown bits.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u16 {
    ///         Foo = 0b0001,
    ///         Bar = 0b0010,
    ///         Baz = 0b0100,
    ///         Qux = 0b1010, // Implies Bar
    ///     }
    /// }
    ///
    /// assert_eq!(FlagSet::<Flag>::new(0b00101), Ok(Flag::Foo | Flag::Baz));
    /// assert_eq!(FlagSet::<Flag>::new(0b01101), Err(flagset::InvalidBits)); // Invalid
    /// assert_eq!(FlagSet::<Flag>::new(0b10101), Err(flagset::InvalidBits)); // Unknown
    /// ```
    #[inline]
    pub fn new(bits: F::Type) -> core::result::Result<Self, InvalidBits> {
        if Self::new_truncated(bits).0 == bits {
            return Ok(FlagSet(bits));
        }

        Err(InvalidBits)
    }

    /// Creates a new set from bits; truncating invalid/unknown bits.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u16 {
    ///         Foo = 0b0001,
    ///         Bar = 0b0010,
    ///         Baz = 0b0100,
    ///         Qux = 0b1010, // Implies Bar
    ///     }
    /// }
    ///
    /// let set = FlagSet::new_truncated(0b11101);  // Has invalid and unknown.
    /// assert_eq!(set, Flag::Foo | Flag::Baz);
    /// assert_eq!(set.bits(), 0b00101);            // Has neither.
    /// ```
    #[inline]
    pub fn new_truncated(bits: F::Type) -> Self {
        let mut set = Self::default();

        for flag in FlagSet::<F>(bits) {
            set |= flag;
        }

        set
    }

    /// Creates a new set from bits; use of invalid/unknown bits is undefined.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u16 {
    ///         Foo = 0b0001,
    ///         Bar = 0b0010,
    ///         Baz = 0b0100,
    ///         Qux = 0b1010, // Implies Bar
    ///     }
    /// }
    ///
    /// // Unknown and invalid bits are retained. Behavior is undefined.
    /// let set = unsafe { FlagSet::<Flag>::new_unchecked(0b11101) };
    /// assert_eq!(set.bits(), 0b11101);
    /// ```
    ///
    /// # Safety
    ///
    /// This constructor doesn't check that the bits are valid. If you pass
    /// undefined flags, undefined behavior may result.
    #[inline]
    pub unsafe fn new_unchecked(bits: F::Type) -> Self {
        FlagSet(bits)
    }

    /// Creates a new FlagSet containing all possible flags.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let set = FlagSet::full();
    /// assert!(!set.is_empty());
    /// assert!(set.is_full());
    /// assert!(set.contains(Flag::Foo));
    /// assert!(set.contains(Flag::Bar));
    /// assert!(set.contains(Flag::Baz));
    /// ```
    #[inline]
    pub fn full() -> Self {
        let mut set = Self::default();
        for f in F::LIST {
            set |= *f
        }
        set
    }

    /// Returns the raw bits of the set.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u16 {
    ///         Foo = 0b0001,
    ///         Bar = 0b0010,
    ///         Baz = 0b0100,
    ///     }
    /// }
    ///
    /// let set = Flag::Foo | Flag::Baz;
    /// assert_eq!(set.bits(), 0b0101u16);
    /// ```
    #[inline]
    pub fn bits(self) -> F::Type {
        self.0
    }

    /// Returns true if the FlagSet contains no flags.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let mut set = Flag::Foo | Flag::Bar;
    /// assert!(!set.is_empty());
    ///
    /// set &= Flag::Baz;
    /// assert!(set.is_empty());
    /// ```
    #[inline]
    pub fn is_empty(self) -> bool {
        self == Self::default()
    }

    /// Returns true if the FlagSet contains all possible flags.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let mut set = Flag::Foo | Flag::Bar;
    /// assert!(!set.is_full());
    ///
    /// set |= Flag::Baz;
    /// assert!(set.is_full());
    /// ```
    #[inline]
    pub fn is_full(self) -> bool {
        self == Self::full()
    }

    /// Returns true if the two `FlagSet`s do not share any flags.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let set = Flag::Foo | Flag::Bar;
    /// assert!(!set.is_disjoint(Flag::Foo));
    /// assert!(!set.is_disjoint(Flag::Foo | Flag::Baz));
    /// assert!(set.is_disjoint(Flag::Baz));
    /// ```
    #[inline]
    pub fn is_disjoint(self, rhs: impl Into<FlagSet<F>>) -> bool {
        self & rhs == Self::default()
    }

    /// Returns true if this FlagSet is a superset of the specified flags.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let set = Flag::Foo | Flag::Bar;
    /// assert!(set.contains(Flag::Foo));
    /// assert!(set.contains(Flag::Foo | Flag::Bar));
    /// assert!(!set.contains(Flag::Foo | Flag::Bar | Flag::Baz));
    /// ```
    #[inline]
    pub fn contains(self, rhs: impl Into<FlagSet<F>>) -> bool {
        let rhs = rhs.into();
        self & rhs == rhs
    }

    /// Removes all flags from the FlagSet.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let mut set = Flag::Foo | Flag::Bar;
    /// assert!(!set.is_empty());
    ///
    /// set.clear();
    /// assert!(set.is_empty());
    /// ```
    #[inline]
    pub fn clear(&mut self) {
        *self = Self::default();
    }

    /// Clears the current set and returns an iterator of all removed flags.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let mut set = Flag::Foo | Flag::Bar;
    /// let mut iter = set.drain();
    /// assert!(set.is_empty());
    /// assert_eq!(iter.next(), Some(Flag::Foo));
    /// assert_eq!(iter.next(), Some(Flag::Bar));
    /// assert_eq!(iter.next(), None);
    /// ```
    #[inline]
    pub fn drain(&mut self) -> Iter<F> {
        let iter = self.into_iter();
        *self = Self::default();
        iter
    }

    /// Retain only the flags flags specified by the predicate.
    ///
    /// ```
    /// use flagset::{FlagSet, flags};
    ///
    /// flags! {
    ///     pub enum Flag: u8 {
    ///         Foo = 1,
    ///         Bar = 2,
    ///         Baz = 4
    ///     }
    /// }
    ///
    /// let mut set0 = Flag::Foo | Flag::Bar;
    /// set0.retain(|f| f != Flag::Foo);
    /// assert_eq!(set0, Flag::Bar);
    /// ```
    #[inline]
    pub fn retain(&mut self, func: impl Fn(F) -> bool) {
        for f in self.into_iter() {
            if !func(f) {
                *self -= f
            }
        }
    }
}

#[cfg(feature = "serde")]
impl<F: Flags> serde::Serialize for FlagSet<F>
where
    F::Type: serde::ser::Serialize,
{
    #[inline]
    fn serialize<S>(&self, serializer: S) -> core::result::Result<S::Ok, S::Error>
    where
        S: serde::ser::Serializer,
    {
        self.0.serialize(serializer)
    }
}

#[cfg(feature = "serde")]
impl<'de, F: Flags> serde::Deserialize<'de> for FlagSet<F>
where
    F::Type: serde::de::Deserialize<'de>,
{
    #[inline]
    fn deserialize<D>(deserializer: D) -> core::result::Result<Self, D::Error>
    where
        D: serde::de::Deserializer<'de>,
    {
        Ok(FlagSet(F::Type::deserialize(deserializer)?))
    }
}

/// Define flag value using the `enum` syntax. See below for details.
///
/// Each enumeration value **MUST** have a specified value.
///
/// The width of the bitfield **MUST** also be specified by its integer type.
///
/// It is important to note that the size of the flag enumeration itself is
/// unrelated to the size of the corresponding `FlagSet` instance.
///
/// It is also worth noting that this macro automatically implements a variety
/// of standard traits including:
///   * Copy
///   * Clone
///   * Debug
///   * PartialEq
///   * Eq
///   * From<$enum> for $integer
///   * Not
///   * BitAnd
///   * BitOr
///   * BitXor
///   * Sub
///   * Rem
///
/// ```
/// use std::mem::{align_of, size_of};
/// use flagset::{FlagSet, flags};
///
/// flags! {
///     enum FlagEmpty: u32 {}
///
///     enum Flag8: u8 {
///         Foo = 0b001,
///         Bar = 0b010,
///         Baz = 0b100
///     }
///
///     pub enum Flag16: u16 {
///         Foo,
///         Bar,
///         #[deprecated]
///         Baz,
///     }
///
///     #[derive(PartialOrd, Ord)]
///     enum Flag32: u32 {
///         Foo = 0b001,
///         #[deprecated]
///         Bar = 0b010,
///         Baz = 0b100
///     }
///
///     #[repr(u64)]
///     enum Flag64: u64 {
///         Foo = 0b001,
///         Bar = 0b010,
///         Baz = 0b100
///     }
///
///     #[repr(u32)]
///     enum Flag128: u128 {
///         Foo = 0b001,
///         Bar = 0b010,
///         Baz = 0b100
///     }
/// }
///
/// assert_eq!(size_of::<Flag8>(), 1);
/// assert_eq!(size_of::<Flag16>(), 1);
/// assert_eq!(size_of::<Flag32>(), 1);
/// assert_eq!(size_of::<Flag64>(), 8);
/// assert_eq!(size_of::<Flag128>(), 4);
///
/// assert_eq!(align_of::<Flag8>(), 1);
/// assert_eq!(align_of::<Flag16>(), 1);
/// assert_eq!(align_of::<Flag32>(), 1);
/// assert_eq!(align_of::<Flag64>(), align_of::<u64>());
/// assert_eq!(align_of::<Flag128>(), align_of::<u32>());
///
/// assert_eq!(size_of::<FlagSet<Flag8>>(), size_of::<u8>());
/// assert_eq!(size_of::<FlagSet<Flag16>>(), size_of::<u16>());
/// assert_eq!(size_of::<FlagSet<Flag32>>(), size_of::<u32>());
/// assert_eq!(size_of::<FlagSet<Flag64>>(), size_of::<u64>());
/// assert_eq!(size_of::<FlagSet<Flag128>>(), size_of::<u128>());
///
/// assert_eq!(align_of::<FlagSet<Flag8>>(), align_of::<u8>());
/// assert_eq!(align_of::<FlagSet<Flag16>>(), align_of::<u16>());
/// assert_eq!(align_of::<FlagSet<Flag32>>(), align_of::<u32>());
/// assert_eq!(align_of::<FlagSet<Flag64>>(), align_of::<u64>());
/// assert_eq!(align_of::<FlagSet<Flag128>>(), align_of::<u128>());
/// ```
#[macro_export]
macro_rules! flags {
    () => {};

    // Entry point for enumerations without values.
    ($(#[$m:meta])* $p:vis enum $n:ident: $t:ty { $($(#[$a:meta])* $k:ident),+ $(,)* } $($next:tt)*) => {
        $crate::flags! { $(#[$m])* $p enum $n: $t { $($(#[$a])* $k = (1 << $n::$k as $t)),+ } $($next)* }
    };

    // Entrypoint for enumerations with values.
    ($(#[$m:meta])* $p:vis enum $n:ident: $t:ty { $($(#[$a:meta])*$k:ident = $v:expr),* $(,)* } $($next:tt)*) => {
        $(#[$m])*
        #[derive(Copy, Clone, Debug, PartialEq, Eq)]
        $p enum $n { $($(#[$a])* $k),* }

        impl $crate::Flags for $n {
            type Type = $t;

            const LIST: &'static [Self] = &[$($n::$k),*];
        }

        impl core::convert::From<$n> for $crate::FlagSet<$n> {
            #[inline]
            fn from(value: $n) -> Self {
                unsafe {
                    match value {
                        $($n::$k => Self::new_unchecked($v)),*
                    }
                }
            }
        }

        impl core::ops::Not for $n {
            type Output = $crate::FlagSet<$n>;

            #[inline]
            fn not(self) -> Self::Output {
                !$crate::FlagSet::from(self)
            }
        }

        impl<R: core::convert::Into<$crate::FlagSet<$n>>> core::ops::BitAnd<R> for $n {
            type Output = $crate::FlagSet<$n>;

            #[inline]
            fn bitand(self, rhs: R) -> Self::Output {
                $crate::FlagSet::from(self) & rhs
            }
        }

        impl<R: core::convert::Into<$crate::FlagSet<$n>>> core::ops::BitOr<R> for $n {
            type Output = $crate::FlagSet<$n>;

            #[inline]
            fn bitor(self, rhs: R) -> Self::Output {
                $crate::FlagSet::from(self) | rhs
            }
        }

        impl<R: core::convert::Into<$crate::FlagSet<$n>>> core::ops::BitXor<R> for $n {
            type Output = $crate::FlagSet<$n>;

            #[inline]
            fn bitxor(self, rhs: R) -> Self::Output {
                $crate::FlagSet::from(self) ^ rhs
            }
        }

        impl<R: core::convert::Into<$crate::FlagSet<$n>>> core::ops::Sub<R> for $n {
            type Output = $crate::FlagSet<$n>;

            #[inline]
            fn sub(self, rhs: R) -> Self::Output {
                $crate::FlagSet::from(self) - rhs
            }
        }

        impl<R: core::convert::Into<$crate::FlagSet<$n>>> core::ops::Rem<R> for $n {
            type Output = $crate::FlagSet<$n>;

            #[inline]
            fn rem(self, rhs: R) -> Self::Output {
                $crate::FlagSet::from(self) % rhs
            }
        }

        $crate::flags! { $($next)* }
    };
}
