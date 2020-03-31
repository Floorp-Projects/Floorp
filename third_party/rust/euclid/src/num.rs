// Copyright 2014 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//! A one-dimensional length, tagged with its units.

use num_traits;

pub trait Zero {
    fn zero() -> Self;
}

impl<T: num_traits::Zero> Zero for T {
    fn zero() -> T {
        num_traits::Zero::zero()
    }
}

pub trait One {
    fn one() -> Self;
}

impl<T: num_traits::One> One for T {
    fn one() -> T {
        num_traits::One::one()
    }
}

/// Defines the nearest integer value to the original value.
pub trait Round: Copy {
    /// Rounds to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    #[must_use]
    fn round(self) -> Self;
}
/// Defines the biggest integer equal or lower than the original value.
pub trait Floor: Copy {
    /// Rounds to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    #[must_use]
    fn floor(self) -> Self;
}
/// Defines the smallest integer equal or greater than the original value.
pub trait Ceil: Copy {
    /// Rounds to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    #[must_use]
    fn ceil(self) -> Self;
}

macro_rules! num_int {
    ($ty:ty) => {
        impl Round for $ty {
            #[inline]
            fn round(self) -> $ty {
                self
            }
        }
        impl Floor for $ty {
            #[inline]
            fn floor(self) -> $ty {
                self
            }
        }
        impl Ceil for $ty {
            #[inline]
            fn ceil(self) -> $ty {
                self
            }
        }
    };
}
macro_rules! num_float {
    ($ty:ty) => {
        impl Round for $ty {
            #[inline]
            fn round(self) -> $ty {
                num_traits::Float::round(self)
            }
        }
        impl Floor for $ty {
            #[inline]
            fn floor(self) -> $ty {
                num_traits::Float::floor(self)
            }
        }
        impl Ceil for $ty {
            #[inline]
            fn ceil(self) -> $ty {
                num_traits::Float::ceil(self)
            }
        }
    };
}

num_int!(i16);
num_int!(u16);
num_int!(i32);
num_int!(u32);
num_int!(i64);
num_int!(u64);
num_int!(isize);
num_int!(usize);
num_float!(f32);
num_float!(f64);
