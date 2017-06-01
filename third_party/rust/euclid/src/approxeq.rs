// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.


/// Trait for testing approximate equality
pub trait ApproxEq<Eps> {
    fn approx_epsilon() -> Eps;
    fn approx_eq(&self, other: &Self) -> bool;
    fn approx_eq_eps(&self, other: &Self, approx_epsilon: &Eps) -> bool;
}

macro_rules! approx_eq {
    ($ty:ty, $eps:expr) => (
        impl ApproxEq<$ty> for $ty {
            #[inline]
            fn approx_epsilon() -> $ty { $eps }
            #[inline]
            fn approx_eq(&self, other: &$ty) -> bool {
                self.approx_eq_eps(other, &$eps)
            }
            #[inline]
            fn approx_eq_eps(&self, other: &$ty, approx_epsilon: &$ty) -> bool {
                (*self - *other).abs() < *approx_epsilon
            }
        }
    )
}

approx_eq!(f32, 1.0e-6);
approx_eq!(f64, 1.0e-6);
