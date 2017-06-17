// Copyright 2014 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.
//! A type-checked scaling factor between units.

use num::One;

use heapsize::HeapSizeOf;
use num_traits::NumCast;
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::fmt;
use std::ops::{Add, Mul, Sub, Div};
use std::marker::PhantomData;

/// A scaling factor between two different units of measurement.
///
/// This is effectively a type-safe float, intended to be used in combination with other types like
/// `length::Length` to enforce conversion between systems of measurement at compile time.
///
/// `Src` and `Dst` represent the units before and after multiplying a value by a `ScaleFactor`. They
/// may be types without values, such as empty enums.  For example:
///
/// ```rust
/// use euclid::ScaleFactor;
/// use euclid::Length;
/// enum Mm {};
/// enum Inch {};
///
/// let mm_per_inch: ScaleFactor<f32, Inch, Mm> = ScaleFactor::new(25.4);
///
/// let one_foot: Length<f32, Inch> = Length::new(12.0);
/// let one_foot_in_mm: Length<f32, Mm> = one_foot * mm_per_inch;
/// ```
#[repr(C)]
pub struct ScaleFactor<T, Src, Dst>(pub T, PhantomData<(Src, Dst)>);

impl<T: HeapSizeOf, Src, Dst> HeapSizeOf for ScaleFactor<T, Src, Dst> {
    fn heap_size_of_children(&self) -> usize {
        self.0.heap_size_of_children()
    }
}

impl<T, Src, Dst> Deserialize for ScaleFactor<T, Src, Dst> where T: Deserialize {
    fn deserialize<D>(deserializer: D) -> Result<ScaleFactor<T, Src, Dst>, D::Error>
                      where D: Deserializer {
        Ok(ScaleFactor(try!(Deserialize::deserialize(deserializer)), PhantomData))
    }
}

impl<T, Src, Dst> Serialize for ScaleFactor<T, Src, Dst> where T: Serialize {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: Serializer {
        self.0.serialize(serializer)
    }
}

impl<T, Src, Dst> ScaleFactor<T, Src, Dst> {
    pub fn new(x: T) -> ScaleFactor<T, Src, Dst> {
        ScaleFactor(x, PhantomData)
    }
}

impl<T: Clone, Src, Dst> ScaleFactor<T, Src, Dst> {
    pub fn get(&self) -> T {
        self.0.clone()
    }
}

impl<T: Clone + One + Div<T, Output=T>, Src, Dst> ScaleFactor<T, Src, Dst> {
    /// The inverse ScaleFactor (1.0 / self).
    pub fn inv(&self) -> ScaleFactor<T, Dst, Src> {
        let one: T = One::one();
        ScaleFactor::new(one / self.get())
    }
}

// scale0 * scale1
impl<T: Clone + Mul<T, Output=T>, A, B, C>
Mul<ScaleFactor<T, B, C>> for ScaleFactor<T, A, B> {
    type Output = ScaleFactor<T, A, C>;
    #[inline]
    fn mul(self, other: ScaleFactor<T, B, C>) -> ScaleFactor<T, A, C> {
        ScaleFactor::new(self.get() * other.get())
    }
}

// scale0 + scale1
impl<T: Clone + Add<T, Output=T>, Src, Dst> Add for ScaleFactor<T, Src, Dst> {
    type Output = ScaleFactor<T, Src, Dst>;
    #[inline]
    fn add(self, other: ScaleFactor<T, Src, Dst>) -> ScaleFactor<T, Src, Dst> {
        ScaleFactor::new(self.get() + other.get())
    }
}

// scale0 - scale1
impl<T: Clone + Sub<T, Output=T>, Src, Dst> Sub for ScaleFactor<T, Src, Dst> {
    type Output = ScaleFactor<T, Src, Dst>;
    #[inline]
    fn sub(self, other: ScaleFactor<T, Src, Dst>) -> ScaleFactor<T, Src, Dst> {
        ScaleFactor::new(self.get() - other.get())
    }
}

impl<T: NumCast + Clone, Src, Dst0> ScaleFactor<T, Src, Dst0> {
    /// Cast from one numeric representation to another, preserving the units.
    pub fn cast<T1: NumCast + Clone>(&self) -> Option<ScaleFactor<T1, Src, Dst0>> {
        NumCast::from(self.get()).map(ScaleFactor::new)
    }
}

// FIXME: Switch to `derive(PartialEq, Clone)` after this Rust issue is fixed:
// https://github.com/mozilla/rust/issues/7671

impl<T: PartialEq, Src, Dst> PartialEq for ScaleFactor<T, Src, Dst> {
    fn eq(&self, other: &ScaleFactor<T, Src, Dst>) -> bool {
        self.0 == other.0
    }
}

impl<T: Clone, Src, Dst> Clone for ScaleFactor<T, Src, Dst> {
    fn clone(&self) -> ScaleFactor<T, Src, Dst> {
        ScaleFactor::new(self.get())
    }
}

impl<T: Copy, Src, Dst> Copy for ScaleFactor<T, Src, Dst> {}

impl<T: fmt::Debug, Src, Dst> fmt::Debug for ScaleFactor<T, Src, Dst> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl<T: fmt::Display, Src, Dst> fmt::Display for ScaleFactor<T, Src, Dst> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

#[cfg(test)]
mod tests {
    use super::ScaleFactor;

    enum Inch {}
    enum Cm {}
    enum Mm {}

    #[test]
    fn test_scale_factor() {
        let mm_per_inch: ScaleFactor<f32, Inch, Mm> = ScaleFactor::new(25.4);
        let cm_per_mm: ScaleFactor<f32, Mm, Cm> = ScaleFactor::new(0.1);

        let mm_per_cm: ScaleFactor<f32, Cm, Mm> = cm_per_mm.inv();
        assert_eq!(mm_per_cm.get(), 10.0);

        let cm_per_inch: ScaleFactor<f32, Inch, Cm> = mm_per_inch * cm_per_mm;
        assert_eq!(cm_per_inch, ScaleFactor::new(2.54));

        let a: ScaleFactor<isize, Inch, Inch> = ScaleFactor::new(2);
        let b: ScaleFactor<isize, Inch, Inch> = ScaleFactor::new(3);
        assert!(a != b);
        assert_eq!(a, a.clone());
        assert_eq!(a.clone() + b.clone(), ScaleFactor::new(5));
        assert_eq!(a - b, ScaleFactor::new(-1));
    }
}
