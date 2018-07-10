// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::UnknownUnit;
use length::Length;
use scale::TypedScale;
use vector::{TypedVector2D, vec2, BoolVector2D};
use num::*;

use num_traits::{NumCast, Signed};
use core::fmt;
use core::ops::{Add, Div, Mul, Sub};
use core::marker::PhantomData;

/// A 2d size tagged with a unit.
define_matrix! {
    pub struct TypedSize2D<T, U> {
        pub width: T,
        pub height: T,
    }
}

/// Default 2d size type with no unit.
///
/// `Size2D` provides the same methods as `TypedSize2D`.
pub type Size2D<T> = TypedSize2D<T, UnknownUnit>;

impl<T: fmt::Debug, U> fmt::Debug for TypedSize2D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}×{:?}", self.width, self.height)
    }
}

impl<T: fmt::Display, U> fmt::Display for TypedSize2D<T, U> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "({}x{})", self.width, self.height)
    }
}

impl<T, U> TypedSize2D<T, U> {
    /// Constructor taking scalar values.
    pub fn new(width: T, height: T) -> Self {
        TypedSize2D {
            width,
            height,
            _unit: PhantomData,
        }
    }
}

impl<T: Clone, U> TypedSize2D<T, U> {
    /// Constructor taking scalar strongly typed lengths.
    pub fn from_lengths(width: Length<T, U>, height: Length<T, U>) -> Self {
        TypedSize2D::new(width.get(), height.get())
    }
}

impl<T: Round, U> TypedSize2D<T, U> {
    /// Rounds each component to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn round(&self) -> Self {
        TypedSize2D::new(self.width.round(), self.height.round())
    }
}

impl<T: Ceil, U> TypedSize2D<T, U> {
    /// Rounds each component to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn ceil(&self) -> Self {
        TypedSize2D::new(self.width.ceil(), self.height.ceil())
    }
}

impl<T: Floor, U> TypedSize2D<T, U> {
    /// Rounds each component to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn floor(&self) -> Self {
        TypedSize2D::new(self.width.floor(), self.height.floor())
    }
}

impl<T: Copy + Add<T, Output = T>, U> Add for TypedSize2D<T, U> {
    type Output = Self;
    fn add(self, other: Self) -> Self {
        TypedSize2D::new(self.width + other.width, self.height + other.height)
    }
}

impl<T: Copy + Sub<T, Output = T>, U> Sub for TypedSize2D<T, U> {
    type Output = Self;
    fn sub(self, other: Self) -> Self {
        TypedSize2D::new(self.width - other.width, self.height - other.height)
    }
}

impl<T: Copy + Clone + Mul<T>, U> TypedSize2D<T, U> {
    pub fn area(&self) -> T::Output {
        self.width * self.height
    }
}

impl<T, U> TypedSize2D<T, U>
where
    T: Copy + One + Add<Output = T> + Sub<Output = T> + Mul<Output = T>,
{
    /// Linearly interpolate between this size and another size.
    ///
    /// `t` is expected to be between zero and one.
    #[inline]
    pub fn lerp(&self, other: Self, t: T) -> Self {
        let one_t = T::one() - t;
        size2(
            one_t * self.width + t * other.width,
            one_t * self.height + t * other.height,
        )
    }
}

impl<T: Zero + PartialOrd, U> TypedSize2D<T, U> {
    pub fn is_empty_or_negative(&self) -> bool {
        let zero = T::zero();
        self.width <= zero || self.height <= zero
    }
}

impl<T: Zero, U> TypedSize2D<T, U> {
    pub fn zero() -> Self {
        TypedSize2D::new(Zero::zero(), Zero::zero())
    }
}

impl<T: Zero, U> Zero for TypedSize2D<T, U> {
    fn zero() -> Self {
        TypedSize2D::new(Zero::zero(), Zero::zero())
    }
}

impl<T: Copy + Mul<T, Output = T>, U> Mul<T> for TypedSize2D<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        TypedSize2D::new(self.width * scale, self.height * scale)
    }
}

impl<T: Copy + Div<T, Output = T>, U> Div<T> for TypedSize2D<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        TypedSize2D::new(self.width / scale, self.height / scale)
    }
}

impl<T: Copy + Mul<T, Output = T>, U1, U2> Mul<TypedScale<T, U1, U2>> for TypedSize2D<T, U1> {
    type Output = TypedSize2D<T, U2>;
    #[inline]
    fn mul(self, scale: TypedScale<T, U1, U2>) -> TypedSize2D<T, U2> {
        TypedSize2D::new(self.width * scale.get(), self.height * scale.get())
    }
}

impl<T: Copy + Div<T, Output = T>, U1, U2> Div<TypedScale<T, U1, U2>> for TypedSize2D<T, U2> {
    type Output = TypedSize2D<T, U1>;
    #[inline]
    fn div(self, scale: TypedScale<T, U1, U2>) -> TypedSize2D<T, U1> {
        TypedSize2D::new(self.width / scale.get(), self.height / scale.get())
    }
}

impl<T: Copy, U> TypedSize2D<T, U> {
    /// Returns self.width as a Length carrying the unit.
    #[inline]
    pub fn width_typed(&self) -> Length<T, U> {
        Length::new(self.width)
    }

    /// Returns self.height as a Length carrying the unit.
    #[inline]
    pub fn height_typed(&self) -> Length<T, U> {
        Length::new(self.height)
    }

    #[inline]
    pub fn to_array(&self) -> [T; 2] {
        [self.width, self.height]
    }

    #[inline]
    pub fn to_vector(&self) -> TypedVector2D<T, U> {
        vec2(self.width, self.height)
    }

    /// Drop the units, preserving only the numeric value.
    pub fn to_untyped(&self) -> Size2D<T> {
        TypedSize2D::new(self.width, self.height)
    }

    /// Tag a unitless value with units.
    pub fn from_untyped(p: &Size2D<T>) -> Self {
        TypedSize2D::new(p.width, p.height)
    }
}

impl<T: NumCast + Copy, Unit> TypedSize2D<T, Unit> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using `round()`, `ceil()` or `floor()` before casting.
    pub fn cast<NewT: NumCast + Copy>(&self) -> TypedSize2D<NewT, Unit> {
        self.try_cast().unwrap()
    }

    /// Fallible cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using `round()`, `ceil()` or `floor()` before casting.
    pub fn try_cast<NewT: NumCast + Copy>(&self) -> Option<TypedSize2D<NewT, Unit>> {
        match (NumCast::from(self.width), NumCast::from(self.height)) {
            (Some(w), Some(h)) => Some(TypedSize2D::new(w, h)),
            _ => None,
        }
    }

    // Convenience functions for common casts

    /// Cast into an `f32` size.
    pub fn to_f32(&self) -> TypedSize2D<f32, Unit> {
        self.cast()
    }

    /// Cast into an `f64` size.
    pub fn to_f64(&self) -> TypedSize2D<f64, Unit> {
        self.cast()
    }

    /// Cast into an `uint` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_usize(&self) -> TypedSize2D<usize, Unit> {
        self.cast()
    }

    /// Cast into an `u32` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_u32(&self) -> TypedSize2D<u32, Unit> {
        self.cast()
    }

    /// Cast into an `i32` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i32(&self) -> TypedSize2D<i32, Unit> {
        self.cast()
    }

    /// Cast into an `i64` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i64(&self) -> TypedSize2D<i64, Unit> {
        self.cast()
    }
}

impl<T, U> TypedSize2D<T, U>
where
    T: Signed,
{
    pub fn abs(&self) -> Self {
        size2(self.width.abs(), self.height.abs())
    }

    pub fn is_positive(&self) -> bool {
        self.width.is_positive() && self.height.is_positive()
    }
}

impl<T: PartialOrd, U> TypedSize2D<T, U> {
    pub fn greater_than(&self, other: &Self) -> BoolVector2D {
        BoolVector2D {
            x: self.width > other.width,
            y: self.height > other.height,
        }
    }

    pub fn lower_than(&self, other: &Self) -> BoolVector2D {
        BoolVector2D {
            x: self.width < other.width,
            y: self.height < other.height,
        }
    }
}


impl<T: PartialEq, U> TypedSize2D<T, U> {
    pub fn equal(&self, other: &Self) -> BoolVector2D {
        BoolVector2D {
            x: self.width == other.width,
            y: self.height == other.height,
        }
    }

    pub fn not_equal(&self, other: &Self) -> BoolVector2D {
        BoolVector2D {
            x: self.width != other.width,
            y: self.height != other.height,
        }
    }
}

/// Shorthand for `TypedSize2D::new(w, h)`.
pub fn size2<T, U>(w: T, h: T) -> TypedSize2D<T, U> {
    TypedSize2D::new(w, h)
}

#[cfg(test)]
mod size2d {
    use super::Size2D;

    #[test]
    pub fn test_add() {
        let p1 = Size2D::new(1.0, 2.0);
        let p2 = Size2D::new(3.0, 4.0);
        assert_eq!(p1 + p2, Size2D::new(4.0, 6.0));

        let p1 = Size2D::new(1.0, 2.0);
        let p2 = Size2D::new(0.0, 0.0);
        assert_eq!(p1 + p2, Size2D::new(1.0, 2.0));

        let p1 = Size2D::new(1.0, 2.0);
        let p2 = Size2D::new(-3.0, -4.0);
        assert_eq!(p1 + p2, Size2D::new(-2.0, -2.0));

        let p1 = Size2D::new(0.0, 0.0);
        let p2 = Size2D::new(0.0, 0.0);
        assert_eq!(p1 + p2, Size2D::new(0.0, 0.0));
    }

    #[test]
    pub fn test_sub() {
        let p1 = Size2D::new(1.0, 2.0);
        let p2 = Size2D::new(3.0, 4.0);
        assert_eq!(p1 - p2, Size2D::new(-2.0, -2.0));

        let p1 = Size2D::new(1.0, 2.0);
        let p2 = Size2D::new(0.0, 0.0);
        assert_eq!(p1 - p2, Size2D::new(1.0, 2.0));

        let p1 = Size2D::new(1.0, 2.0);
        let p2 = Size2D::new(-3.0, -4.0);
        assert_eq!(p1 - p2, Size2D::new(4.0, 6.0));

        let p1 = Size2D::new(0.0, 0.0);
        let p2 = Size2D::new(0.0, 0.0);
        assert_eq!(p1 - p2, Size2D::new(0.0, 0.0));
    }

    #[test]
    pub fn test_area() {
        let p = Size2D::new(1.5, 2.0);
        assert_eq!(p.area(), 3.0);
    }
}
