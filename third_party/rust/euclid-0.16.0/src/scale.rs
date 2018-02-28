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

use num_traits::NumCast;
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::fmt;
use std::ops::{Add, Mul, Sub, Div, Neg};
use std::marker::PhantomData;
use {TypedRect, TypedSize2D, TypedPoint2D, TypedVector2D};

/// A scaling factor between two different units of measurement.
///
/// This is effectively a type-safe float, intended to be used in combination with other types like
/// `length::Length` to enforce conversion between systems of measurement at compile time.
///
/// `Src` and `Dst` represent the units before and after multiplying a value by a `TypedScale`. They
/// may be types without values, such as empty enums.  For example:
///
/// ```rust
/// use euclid::TypedScale;
/// use euclid::Length;
/// enum Mm {};
/// enum Inch {};
///
/// let mm_per_inch: TypedScale<f32, Inch, Mm> = TypedScale::new(25.4);
///
/// let one_foot: Length<f32, Inch> = Length::new(12.0);
/// let one_foot_in_mm: Length<f32, Mm> = one_foot * mm_per_inch;
/// ```
#[repr(C)]
pub struct TypedScale<T, Src, Dst>(pub T, PhantomData<(Src, Dst)>);

impl<'de, T, Src, Dst> Deserialize<'de> for TypedScale<T, Src, Dst> where T: Deserialize<'de> {
    fn deserialize<D>(deserializer: D) -> Result<TypedScale<T, Src, Dst>, D::Error>
                      where D: Deserializer<'de> {
        Ok(TypedScale(try!(Deserialize::deserialize(deserializer)), PhantomData))
    }
}

impl<T, Src, Dst> Serialize for TypedScale<T, Src, Dst> where T: Serialize {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error> where S: Serializer {
        self.0.serialize(serializer)
    }
}

impl<T, Src, Dst> TypedScale<T, Src, Dst> {
    pub fn new(x: T) -> TypedScale<T, Src, Dst> {
        TypedScale(x, PhantomData)
    }
}

impl<T: Clone, Src, Dst> TypedScale<T, Src, Dst> {
    pub fn get(&self) -> T {
        self.0.clone()
    }
}

impl<T: Clone + One + Div<T, Output=T>, Src, Dst> TypedScale<T, Src, Dst> {
    /// The inverse TypedScale (1.0 / self).
    pub fn inv(&self) -> TypedScale<T, Dst, Src> {
        let one: T = One::one();
        TypedScale::new(one / self.get())
    }
}

// scale0 * scale1
impl<T: Clone + Mul<T, Output=T>, A, B, C>
Mul<TypedScale<T, B, C>> for TypedScale<T, A, B> {
    type Output = TypedScale<T, A, C>;
    #[inline]
    fn mul(self, other: TypedScale<T, B, C>) -> TypedScale<T, A, C> {
        TypedScale::new(self.get() * other.get())
    }
}

// scale0 + scale1
impl<T: Clone + Add<T, Output=T>, Src, Dst> Add for TypedScale<T, Src, Dst> {
    type Output = TypedScale<T, Src, Dst>;
    #[inline]
    fn add(self, other: TypedScale<T, Src, Dst>) -> TypedScale<T, Src, Dst> {
        TypedScale::new(self.get() + other.get())
    }
}

// scale0 - scale1
impl<T: Clone + Sub<T, Output=T>, Src, Dst> Sub for TypedScale<T, Src, Dst> {
    type Output = TypedScale<T, Src, Dst>;
    #[inline]
    fn sub(self, other: TypedScale<T, Src, Dst>) -> TypedScale<T, Src, Dst> {
        TypedScale::new(self.get() - other.get())
    }
}

impl<T: NumCast + Clone, Src, Dst0> TypedScale<T, Src, Dst0> {
    /// Cast from one numeric representation to another, preserving the units.
    pub fn cast<T1: NumCast + Clone>(&self) -> Option<TypedScale<T1, Src, Dst0>> {
        NumCast::from(self.get()).map(TypedScale::new)
    }
}

impl<T, Src, Dst> TypedScale<T, Src, Dst>
where T: Copy + Clone +
         Mul<T, Output=T> +
         Neg<Output=T> +
         PartialEq +
         One
{
    /// Returns the given point transformed by this scale.
    #[inline]
    pub fn transform_point(&self, point: &TypedPoint2D<T, Src>) -> TypedPoint2D<T, Dst> {
        TypedPoint2D::new(point.x * self.get(), point.y * self.get())
    }

    /// Returns the given vector transformed by this scale.
    #[inline]
    pub fn transform_vector(&self, vec: &TypedVector2D<T, Src>) -> TypedVector2D<T, Dst> {
        TypedVector2D::new(vec.x * self.get(), vec.y * self.get())
    }

    /// Returns the given vector transformed by this scale.
    #[inline]
    pub fn transform_size(&self, size: &TypedSize2D<T, Src>) -> TypedSize2D<T, Dst> {
        TypedSize2D::new(size.width * self.get(), size.height * self.get())
    }

    /// Returns the given rect transformed by this scale.
    #[inline]
    pub fn transform_rect(&self, rect: &TypedRect<T, Src>) -> TypedRect<T, Dst> {
        TypedRect::new(
            self.transform_point(&rect.origin),
            self.transform_size(&rect.size),
        )
    }

    /// Returns the inverse of this scale.
    #[inline]
    pub fn inverse(&self) -> TypedScale<T, Dst, Src> {
        TypedScale::new(-self.get())
    }

    /// Returns true if this scale has no effect.
    #[inline]
    pub fn is_identity(&self) -> bool {
        self.get() == T::one()
    }
}

// FIXME: Switch to `derive(PartialEq, Clone)` after this Rust issue is fixed:
// https://github.com/mozilla/rust/issues/7671

impl<T: PartialEq, Src, Dst> PartialEq for TypedScale<T, Src, Dst> {
    fn eq(&self, other: &TypedScale<T, Src, Dst>) -> bool {
        self.0 == other.0
    }
}

impl<T: Clone, Src, Dst> Clone for TypedScale<T, Src, Dst> {
    fn clone(&self) -> TypedScale<T, Src, Dst> {
        TypedScale::new(self.get())
    }
}

impl<T: Copy, Src, Dst> Copy for TypedScale<T, Src, Dst> {}

impl<T: fmt::Debug, Src, Dst> fmt::Debug for TypedScale<T, Src, Dst> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl<T: fmt::Display, Src, Dst> fmt::Display for TypedScale<T, Src, Dst> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

#[cfg(test)]
mod tests {
    use super::TypedScale;

    enum Inch {}
    enum Cm {}
    enum Mm {}

    #[test]
    fn test_scale() {
        let mm_per_inch: TypedScale<f32, Inch, Mm> = TypedScale::new(25.4);
        let cm_per_mm: TypedScale<f32, Mm, Cm> = TypedScale::new(0.1);

        let mm_per_cm: TypedScale<f32, Cm, Mm> = cm_per_mm.inv();
        assert_eq!(mm_per_cm.get(), 10.0);

        let cm_per_inch: TypedScale<f32, Inch, Cm> = mm_per_inch * cm_per_mm;
        assert_eq!(cm_per_inch, TypedScale::new(2.54));

        let a: TypedScale<isize, Inch, Inch> = TypedScale::new(2);
        let b: TypedScale<isize, Inch, Inch> = TypedScale::new(3);
        assert!(a != b);
        assert_eq!(a, a.clone());
        assert_eq!(a.clone() + b.clone(), TypedScale::new(5));
        assert_eq!(a - b, TypedScale::new(-1));
    }
}
