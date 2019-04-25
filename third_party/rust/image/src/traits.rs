//! This module provides useful traits that were deprecated in rust

// Note copied from the stdlib under MIT license

use num_traits::{Bounded, Num, NumCast};
use std::ops::AddAssign;

/// Primitive trait from old stdlib
pub trait Primitive: Copy + NumCast + Num + PartialOrd<Self> + Clone + Bounded {}

impl Primitive for usize {}
impl Primitive for u8 {}
impl Primitive for u16 {}
impl Primitive for u32 {}
impl Primitive for u64 {}
impl Primitive for isize {}
impl Primitive for i8 {}
impl Primitive for i16 {}
impl Primitive for i32 {}
impl Primitive for i64 {}
impl Primitive for f32 {}
impl Primitive for f64 {}

/// An Enlargable::Larger value should be enough to calculate
/// the sum (average) of a few hundred or thousand Enlargeable values.
pub trait Enlargeable: Sized + Bounded + NumCast {
    type Larger: Primitive + AddAssign + 'static;

    fn clamp_from(n: Self::Larger) -> Self {
        // Note: Only unsigned value types supported.
        if n > NumCast::from(Self::max_value()).unwrap() {
            Self::max_value()
        } else {
            NumCast::from(n).unwrap()
        }
    }
}

impl Enlargeable for u8 {
    type Larger = u32;
}
impl Enlargeable for u16 {
    type Larger = u32;
}
impl Enlargeable for u32 {
    type Larger = u64;
}
