// SPDX-FileCopyrightText: 2017 - 2023 Kamila Borowska <kamila@borowska.pw>
// SPDX-FileCopyrightText: 2021 Bruno Corrêa Zimmermann <brunoczim@gmail.com>
// SPDX-FileCopyrightText: 2022 philipp <descpl@yahoo.de>
//
// SPDX-License-Identifier: MIT OR Apache-2.0

use core::cmp::Ordering;
use core::convert::Infallible;

/// Enum mapping type.
///
/// This trait is implemented by `#[derive(Enum)]`.
///
/// This trait is also implemented by `bool` and `u8`. While `u8` is
/// strictly speaking not an actual enum, there are good reasons to consider
/// it like one, as array of `u8` keys is a relatively common pattern.
pub trait Enum: Sized {
    /// Length of the enum.
    const LENGTH: usize;

    /// Takes an usize, and returns an element matching `into_usize` function.
    fn from_usize(value: usize) -> Self;
    /// Returns an unique identifier for a value within range of `0..Array::LENGTH`.
    fn into_usize(self) -> usize;
}

/// Trait associating enum with an array.
///
/// This exists due to limitations of Rust compiler that prevent arrays from using
/// associated constants in structures. The array length must match `LENGTH` of an
/// `Enum`.
pub trait EnumArray<V>: Enum {
    /// Representation of an enum map for type `V`.
    type Array: Array<V>;
}

/// Array for enum-map storage.
///
/// This trait is inteded for primitive array types (with fixed length).
///
/// # Safety
///
/// The array length needs to match actual storage.
pub unsafe trait Array<V> {
    // This is necessary duplication because the length in Enum trait can be
    // provided by user and may not be trustworthy for unsafe code.
    const LENGTH: usize;
}

unsafe impl<V, const N: usize> Array<V> for [V; N] {
    const LENGTH: usize = N;
}

#[doc(hidden)]
#[inline]
pub fn out_of_bounds() -> ! {
    panic!("index out of range for Enum::from_usize");
}

impl Enum for bool {
    const LENGTH: usize = 2;

    #[inline]
    fn from_usize(value: usize) -> Self {
        match value {
            0 => false,
            1 => true,
            _ => out_of_bounds(),
        }
    }
    #[inline]
    fn into_usize(self) -> usize {
        usize::from(self)
    }
}

impl<T> EnumArray<T> for bool {
    type Array = [T; Self::LENGTH];
}

impl Enum for () {
    const LENGTH: usize = 1;

    #[inline]
    fn from_usize(value: usize) -> Self {
        match value {
            0 => (),
            _ => out_of_bounds(),
        }
    }
    #[inline]
    fn into_usize(self) -> usize {
        0
    }
}

impl<T> EnumArray<T> for () {
    type Array = [T; Self::LENGTH];
}

impl Enum for u8 {
    const LENGTH: usize = 256;

    #[inline]
    fn from_usize(value: usize) -> Self {
        value.try_into().unwrap_or_else(|_| out_of_bounds())
    }
    #[inline]
    fn into_usize(self) -> usize {
        usize::from(self)
    }
}

impl<T> EnumArray<T> for u8 {
    type Array = [T; Self::LENGTH];
}

impl Enum for Infallible {
    const LENGTH: usize = 0;

    #[inline]
    fn from_usize(_: usize) -> Self {
        out_of_bounds();
    }
    #[inline]
    fn into_usize(self) -> usize {
        match self {}
    }
}

impl<T> EnumArray<T> for Infallible {
    type Array = [T; Self::LENGTH];
}

impl Enum for Ordering {
    const LENGTH: usize = 3;

    #[inline]
    fn from_usize(value: usize) -> Self {
        match value {
            0 => Ordering::Less,
            1 => Ordering::Equal,
            2 => Ordering::Greater,
            _ => out_of_bounds(),
        }
    }
    #[inline]
    fn into_usize(self) -> usize {
        match self {
            Ordering::Less => 0,
            Ordering::Equal => 1,
            Ordering::Greater => 2,
        }
    }
}

impl<T> EnumArray<T> for Ordering {
    type Array = [T; Self::LENGTH];
}
