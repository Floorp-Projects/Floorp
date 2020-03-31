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
#[cfg(feature = "serde")]
use serde;
use core::fmt;
use core::ops::{Add, Div, Mul, Neg, Sub};
use core::marker::PhantomData;
use {Point2D, Rect, Size2D, Vector2D};

/// A scaling factor between two different units of measurement.
///
/// This is effectively a type-safe float, intended to be used in combination with other types like
/// `length::Length` to enforce conversion between systems of measurement at compile time.
///
/// `Src` and `Dst` represent the units before and after multiplying a value by a `Scale`. They
/// may be types without values, such as empty enums.  For example:
///
/// ```rust
/// use euclid::Scale;
/// use euclid::Length;
/// enum Mm {};
/// enum Inch {};
///
/// let mm_per_inch: Scale<f32, Inch, Mm> = Scale::new(25.4);
///
/// let one_foot: Length<f32, Inch> = Length::new(12.0);
/// let one_foot_in_mm: Length<f32, Mm> = one_foot * mm_per_inch;
/// ```
#[repr(C)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(bound(serialize = "T: serde::Serialize", deserialize = "T: serde::Deserialize<'de>")))]
pub struct Scale<T, Src, Dst>(pub T, #[doc(hidden)] pub PhantomData<(Src, Dst)>);

impl<T, Src, Dst> Scale<T, Src, Dst> {
    #[inline]
    pub const fn new(x: T) -> Self {
        Scale(x, PhantomData)
    }
}

impl<T: Clone, Src, Dst> Scale<T, Src, Dst> {
    #[inline]
    pub fn get(&self) -> T {
        self.0.clone()
    }
}

impl<Src, Dst> Scale<f32, Src, Dst> {
    /// Identity scaling, could be used to safely transit from one space to another.
    pub const ONE: Self = Scale(1.0, PhantomData);
}

impl<T: Clone + One + Div<T, Output = T>, Src, Dst> Scale<T, Src, Dst> {
    /// The inverse Scale (1.0 / self).
    ///
    /// # Example
    ///
    /// ```rust
    /// use euclid::Scale;
    /// enum Mm {};
    /// enum Cm {};
    ///
    /// let cm_per_mm: Scale<f32, Cm, Mm> = Scale::new(0.1);
    ///
    /// assert_eq!(cm_per_mm.inv(), Scale::new(10.0));
    /// ```
    pub fn inv(&self) -> Scale<T, Dst, Src> {
        let one: T = One::one();
        Scale::new(one / self.get())
    }
}

// scale0 * scale1
impl<T: Mul<T, Output = T>, A, B, C> Mul<Scale<T, B, C>> for Scale<T, A, B> {
    type Output = Scale<T, A, C>;
    #[inline]
    fn mul(self, other: Scale<T, B, C>) -> Scale<T, A, C> {
        Scale::new(self.0 * other.0)
    }
}

// scale0 + scale1
impl<T: Add<T, Output = T>, Src, Dst> Add for Scale<T, Src, Dst> {
    type Output = Scale<T, Src, Dst>;
    #[inline]
    fn add(self, other: Scale<T, Src, Dst>) -> Scale<T, Src, Dst> {
        Scale::new(self.0 + other.0)
    }
}

// scale0 - scale1
impl<T: Sub<T, Output = T>, Src, Dst> Sub for Scale<T, Src, Dst> {
    type Output = Scale<T, Src, Dst>;
    #[inline]
    fn sub(self, other: Scale<T, Src, Dst>) -> Scale<T, Src, Dst> {
        Scale::new(self.0 - other.0)
    }
}

impl<T: NumCast + Clone, Src, Dst> Scale<T, Src, Dst> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// # Panics
    ///
    /// If the source value cannot be represented by the target type `NewT`, then
    /// method panics. Use `try_cast` if that must be case.
    ///
    /// # Example
    ///
    /// ```rust
    /// use euclid::Scale;
    /// enum Mm {};
    /// enum Cm {};
    ///
    /// let to_mm: Scale<i32, Cm, Mm> = Scale::new(10);
    ///
    /// assert_eq!(to_mm.cast::<f32>(), Scale::new(10.0));
    /// ```
    /// That conversion will panic, because `i32` not enough to store such big numbers:
    /// ```rust,should_panic
    /// use euclid::Scale;
    /// enum Mm {};// millimeter = 10^-2 meters
    /// enum Em {};// exameter   = 10^18 meters
    ///
    /// // Panics
    /// let to_em: Scale<i32, Mm, Em> = Scale::new(10e20).cast();
    /// ```
    #[inline]
    pub fn cast<NewT: NumCast>(&self) -> Scale<NewT, Src, Dst> {
        self.try_cast().unwrap()
    }

    /// Fallible cast from one numeric representation to another, preserving the units.
    /// If the source value cannot be represented by the target type `NewT`, then `None`
    /// is returned.
    ///
    /// # Example
    ///
    /// ```rust
    /// use euclid::Scale;
    /// enum Mm {};
    /// enum Cm {};
    /// enum Em {};// Exameter = 10^18 meters
    ///
    /// let to_mm: Scale<i32, Cm, Mm> = Scale::new(10);
    /// let to_em: Scale<f32, Mm, Em> = Scale::new(10e20);
    ///
    /// assert_eq!(to_mm.try_cast::<f32>(), Some(Scale::new(10.0)));
    /// // Integer to small to store that number
    /// assert_eq!(to_em.try_cast::<i32>(), None);
    /// ```
    pub fn try_cast<NewT: NumCast>(&self) -> Option<Scale<NewT, Src, Dst>> {
        NumCast::from(self.get()).map(Scale::new)
    }
}

impl<T, Src, Dst> Scale<T, Src, Dst>
where
    T: Copy + Mul<T, Output = T> + Neg<Output = T> + PartialEq + One,
{
    /// Returns the given point transformed by this scale.
    ///
    /// # Example
    ///
    /// ```rust
    /// use euclid::{Scale, point2};
    /// enum Mm {};
    /// enum Cm {};
    ///
    /// let to_mm: Scale<i32, Cm, Mm> = Scale::new(10);
    ///
    /// assert_eq!(to_mm.transform_point(point2(42, -42)), point2(420, -420));
    /// ```
    #[inline]
    pub fn transform_point(&self, point: Point2D<T, Src>) -> Point2D<T, Dst> {
        Point2D::new(point.x * self.get(), point.y * self.get())
    }

    /// Returns the given vector transformed by this scale.
    ///
    /// # Example
    ///
    /// ```rust
    /// use euclid::{Scale, vec2};
    /// enum Mm {};
    /// enum Cm {};
    ///
    /// let to_mm: Scale<i32, Cm, Mm> = Scale::new(10);
    ///
    /// assert_eq!(to_mm.transform_vector(vec2(42, -42)), vec2(420, -420));
    /// ```
    #[inline]
    pub fn transform_vector(&self, vec: Vector2D<T, Src>) -> Vector2D<T, Dst> {
        Vector2D::new(vec.x * self.get(), vec.y * self.get())
    }

    /// Returns the given vector transformed by this scale.
    ///
    /// # Example
    ///
    /// ```rust
    /// use euclid::{Scale, size2};
    /// enum Mm {};
    /// enum Cm {};
    ///
    /// let to_mm: Scale<i32, Cm, Mm> = Scale::new(10);
    ///
    /// assert_eq!(to_mm.transform_size(size2(42, -42)), size2(420, -420));
    /// ```
    #[inline]
    pub fn transform_size(&self, size: Size2D<T, Src>) -> Size2D<T, Dst> {
        Size2D::new(size.width * self.get(), size.height * self.get())
    }

    /// Returns the given rect transformed by this scale.
    ///
    /// # Example
    ///
    /// ```rust
    /// use euclid::{Scale, rect};
    /// enum Mm {};
    /// enum Cm {};
    ///
    /// let to_mm: Scale<i32, Cm, Mm> = Scale::new(10);
    ///
    /// assert_eq!(to_mm.transform_rect(&rect(1, 2, 42, -42)), rect(10, 20, 420, -420));
    /// ```
    #[inline]
    pub fn transform_rect(&self, rect: &Rect<T, Src>) -> Rect<T, Dst> {
        Rect::new(
            self.transform_point(rect.origin),
            self.transform_size(rect.size),
        )
    }

    /// Returns the inverse of this scale.
    #[inline]
    pub fn inverse(&self) -> Scale<T, Dst, Src> {
        Scale::new(-self.get())
    }

    /// Returns `true` if this scale has no effect.
    ///
    /// # Example
    ///
    /// ```rust
    /// use euclid::Scale;
    /// use euclid::num::One;
    /// enum Mm {};
    /// enum Cm {};
    ///
    /// let cm_per_mm: Scale<f32, Mm, Cm> = Scale::new(0.1);
    /// let mm_per_mm: Scale<f32, Mm, Mm> = Scale::new(1.0);
    ///
    /// assert_eq!(cm_per_mm.is_identity(), false);
    /// assert_eq!(mm_per_mm.is_identity(), true);
    /// ```
    #[inline]
    pub fn is_identity(&self) -> bool {
        self.0 == T::one()
    }
}

// FIXME: Switch to `derive(PartialEq, Clone)` after this Rust issue is fixed:
// https://github.com/rust-lang/rust/issues/26925

impl<T: PartialEq, Src, Dst> PartialEq for Scale<T, Src, Dst> {
    fn eq(&self, other: &Scale<T, Src, Dst>) -> bool {
        self.0 == other.0
    }
}

impl<T: Clone, Src, Dst> Clone for Scale<T, Src, Dst> {
    fn clone(&self) -> Scale<T, Src, Dst> {
        Scale::new(self.get())
    }
}

impl<T: Copy, Src, Dst> Copy for Scale<T, Src, Dst> {}

impl<T: fmt::Debug, Src, Dst> fmt::Debug for Scale<T, Src, Dst> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl<T: fmt::Display, Src, Dst> fmt::Display for Scale<T, Src, Dst> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

#[cfg(test)]
mod tests {
    use super::Scale;

    enum Inch {}
    enum Cm {}
    enum Mm {}

    #[test]
    fn test_scale() {
        let mm_per_inch: Scale<f32, Inch, Mm> = Scale::new(25.4);
        let cm_per_mm: Scale<f32, Mm, Cm> = Scale::new(0.1);

        let mm_per_cm: Scale<f32, Cm, Mm> = cm_per_mm.inv();
        assert_eq!(mm_per_cm.get(), 10.0);

        let cm_per_inch: Scale<f32, Inch, Cm> = mm_per_inch * cm_per_mm;
        assert_eq!(cm_per_inch, Scale::new(2.54));

        let a: Scale<isize, Inch, Inch> = Scale::new(2);
        let b: Scale<isize, Inch, Inch> = Scale::new(3);
        assert!(a != b);
        assert_eq!(a, a.clone());
        assert_eq!(a.clone() + b.clone(), Scale::new(5));
        assert_eq!(a - b, Scale::new(-1));
    }
}
