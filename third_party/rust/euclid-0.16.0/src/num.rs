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
    fn zero() -> T { num_traits::Zero::zero() }
}

pub trait One {
    fn one() -> Self;
}

impl<T: num_traits::One> One for T {
    fn one() -> T { num_traits::One::one() }
}


pub trait Round : Copy { fn round(self) -> Self; }
pub trait Floor : Copy { fn floor(self) -> Self; }
pub trait Ceil : Copy { fn ceil(self) -> Self; }

macro_rules! num_int {
    ($ty:ty) => (
        impl Round for $ty {
            #[inline]
            fn round(self) -> $ty { self }
        }
        impl Floor for $ty {
            #[inline]
            fn floor(self) -> $ty { self }
        }
        impl Ceil for $ty {
            #[inline]
            fn ceil(self) -> $ty { self }
        }
    )
}
macro_rules! num_float {
    ($ty:ty) => (
        impl Round for $ty {
            #[inline]
            fn round(self) -> $ty { self.round() }
        }
        impl Floor for $ty {
            #[inline]
            fn floor(self) -> $ty { self.floor() }
        }
        impl Ceil for $ty {
            #[inline]
            fn ceil(self) -> $ty { self.ceil() }
        }
    )
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
