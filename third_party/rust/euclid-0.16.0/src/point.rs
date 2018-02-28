// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::UnknownUnit;
use approxeq::ApproxEq;
use length::Length;
use scale::TypedScale;
use size::TypedSize2D;
use num::*;
use num_traits::{Float, NumCast};
use vector::{TypedVector2D, TypedVector3D, vec2, vec3};
use std::fmt;
use std::ops::{Add, Mul, Sub, Div, AddAssign, SubAssign, MulAssign, DivAssign};
use std::marker::PhantomData;

define_matrix! {
    /// A 2d Point tagged with a unit.
    pub struct TypedPoint2D<T, U> {
        pub x: T,
        pub y: T,
    }
}

/// Default 2d point type with no unit.
///
/// `Point2D` provides the same methods as `TypedPoint2D`.
pub type Point2D<T> = TypedPoint2D<T, UnknownUnit>;

impl<T: Copy + Zero, U> TypedPoint2D<T, U> {
    /// Constructor, setting all components to zero.
    #[inline]
    pub fn origin() -> Self {
        point2(Zero::zero(), Zero::zero())
    }

    #[inline]
    pub fn zero() -> Self {
        Self::origin()
    }

    /// Convert into a 3d point.
    #[inline]
    pub fn to_3d(&self) -> TypedPoint3D<T, U> {
        point3(self.x, self.y, Zero::zero())
    }
}

impl<T: fmt::Debug, U> fmt::Debug for TypedPoint2D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({:?},{:?})", self.x, self.y)
    }
}

impl<T: fmt::Display, U> fmt::Display for TypedPoint2D<T, U> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "({},{})", self.x, self.y)
    }
}

impl<T: Copy, U> TypedPoint2D<T, U> {
    /// Constructor taking scalar values directly.
    #[inline]
    pub fn new(x: T, y: T) -> Self {
        TypedPoint2D { x: x, y: y, _unit: PhantomData }
    }

    /// Constructor taking properly typed Lengths instead of scalar values.
    #[inline]
    pub fn from_lengths(x: Length<T, U>, y: Length<T, U>) -> Self {
        point2(x.0, y.0)
    }

    /// Create a 3d point from this one, using the specified z value.
    #[inline]
    pub fn extend(&self, z: T) -> TypedPoint3D<T, U> {
        point3(self.x, self.y, z)
    }

    /// Cast this point into a vector.
    ///
    /// Equivalent to subtracting the origin from this point.
    #[inline]
    pub fn to_vector(&self) -> TypedVector2D<T, U> {
        vec2(self.x, self.y)
    }

    /// Swap x and y.
    #[inline]
    pub fn yx(&self) -> Self {
        point2(self.y, self.x)
    }

    /// Returns self.x as a Length carrying the unit.
    #[inline]
    pub fn x_typed(&self) -> Length<T, U> { Length::new(self.x) }

    /// Returns self.y as a Length carrying the unit.
    #[inline]
    pub fn y_typed(&self) -> Length<T, U> { Length::new(self.y) }

    /// Drop the units, preserving only the numeric value.
    #[inline]
    pub fn to_untyped(&self) -> Point2D<T> {
        point2(self.x, self.y)
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(p: &Point2D<T>) -> Self {
        point2(p.x, p.y)
    }

    #[inline]
    pub fn to_array(&self) -> [T; 2] {
        [self.x, self.y]
    }
}

impl<T: Copy + Add<T, Output=T>, U> TypedPoint2D<T, U> {
    #[inline]
    pub fn add_size(&self, other: &TypedSize2D<T, U>) -> Self {
        point2(self.x + other.width, self.y + other.height)
    }
}

impl<T: Copy + Add<T, Output=T>, U> Add<TypedSize2D<T, U>> for TypedPoint2D<T, U> {
    type Output = Self;
    #[inline]
    fn add(self, other: TypedSize2D<T, U>) -> Self {
        point2(self.x + other.width, self.y + other.height)
    }
}

impl<T: Copy + Add<T, Output=T>, U> AddAssign<TypedVector2D<T, U>> for TypedPoint2D<T, U> {
    #[inline]
    fn add_assign(&mut self, other: TypedVector2D<T, U>) {
        *self = *self + other
    }
}

impl<T: Copy + Sub<T, Output=T>, U> SubAssign<TypedVector2D<T, U>> for TypedPoint2D<T, U> {
    #[inline]
    fn sub_assign(&mut self, other: TypedVector2D<T, U>) {
        *self = *self - other
    }
}

impl<T: Copy + Add<T, Output=T>, U> Add<TypedVector2D<T, U>> for TypedPoint2D<T, U> {
    type Output = Self;
    #[inline]
    fn add(self, other: TypedVector2D<T, U>) -> Self {
        point2(self.x + other.x, self.y + other.y)
    }
}

impl<T: Copy + Sub<T, Output=T>, U> Sub for TypedPoint2D<T, U> {
    type Output = TypedVector2D<T, U>;
    #[inline]
    fn sub(self, other: Self) -> TypedVector2D<T, U> {
        vec2(self.x - other.x, self.y - other.y)
    }
}

impl<T: Copy + Sub<T, Output=T>, U> Sub<TypedVector2D<T, U>> for TypedPoint2D<T, U> {
    type Output = Self;
    #[inline]
    fn sub(self, other: TypedVector2D<T, U>) -> Self {
        point2(self.x - other.x, self.y - other.y)
    }
}

impl<T: Float, U> TypedPoint2D<T, U> {
    #[inline]
    pub fn min(self, other: Self) -> Self {
         point2(self.x.min(other.x), self.y.min(other.y))
    }

    #[inline]
    pub fn max(self, other: Self) -> Self {
        point2(self.x.max(other.x), self.y.max(other.y))
    }
}

impl<T: Copy + Mul<T, Output=T>, U> Mul<T> for TypedPoint2D<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        point2(self.x * scale, self.y * scale)
    }
}

impl<T: Copy + Mul<T, Output=T>, U> MulAssign<T> for TypedPoint2D<T, U> {
    #[inline]
    fn mul_assign(&mut self, scale: T) {
        *self = *self * scale
    }
}

impl<T: Copy + Div<T, Output=T>, U> Div<T> for TypedPoint2D<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        point2(self.x / scale, self.y / scale)
    }
}

impl<T: Copy + Div<T, Output=T>, U> DivAssign<T> for TypedPoint2D<T, U> {
    #[inline]
    fn div_assign(&mut self, scale: T) {
        *self = *self / scale
    }
}

impl<T: Copy + Mul<T, Output=T>, U1, U2> Mul<TypedScale<T, U1, U2>> for TypedPoint2D<T, U1> {
    type Output = TypedPoint2D<T, U2>;
    #[inline]
    fn mul(self, scale: TypedScale<T, U1, U2>) -> TypedPoint2D<T, U2> {
        point2(self.x * scale.get(), self.y * scale.get())
    }
}

impl<T: Copy + Div<T, Output=T>, U1, U2> Div<TypedScale<T, U1, U2>> for TypedPoint2D<T, U2> {
    type Output = TypedPoint2D<T, U1>;
    #[inline]
    fn div(self, scale: TypedScale<T, U1, U2>) -> TypedPoint2D<T, U1> {
        point2(self.x / scale.get(), self.y / scale.get())
    }
}

impl<T: Round, U> TypedPoint2D<T, U> {
    /// Rounds each component to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    /// For example `{ -0.1, -0.8 }.round() == { 0.0, -1.0 }`.
    #[inline]
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn round(&self) -> Self {
        point2(self.x.round(), self.y.round())
    }
}

impl<T: Ceil, U> TypedPoint2D<T, U> {
    /// Rounds each component to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    /// For example `{ -0.1, -0.8 }.ceil() == { 0.0, 0.0 }`.
    #[inline]
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn ceil(&self) -> Self {
        point2(self.x.ceil(), self.y.ceil())
    }
}

impl<T: Floor, U> TypedPoint2D<T, U> {
    /// Rounds each component to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    /// For example `{ -0.1, -0.8 }.floor() == { -1.0, -1.0 }`.
    #[inline]
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn floor(&self) -> Self {
        point2(self.x.floor(), self.y.floor())
    }
}

impl<T: NumCast + Copy, U> TypedPoint2D<T, U> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using `round()`, `ceil()` or `floor()` before casting.
    #[inline]
    pub fn cast<NewT: NumCast + Copy>(&self) -> Option<TypedPoint2D<NewT, U>> {
        match (NumCast::from(self.x), NumCast::from(self.y)) {
            (Some(x), Some(y)) => Some(point2(x, y)),
            _ => None
        }
    }

    // Convenience functions for common casts

    /// Cast into an `f32` point.
    #[inline]
    pub fn to_f32(&self) -> TypedPoint2D<f32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `f64` point.
    #[inline]
    pub fn to_f64(&self) -> TypedPoint2D<f64, U> {
        self.cast().unwrap()
    }

    /// Cast into an `usize` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_usize(&self) -> TypedPoint2D<usize, U> {
        self.cast().unwrap()
    }

    /// Cast into an i32 point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_i32(&self) -> TypedPoint2D<i32, U> {
        self.cast().unwrap()
    }

    /// Cast into an i64 point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_i64(&self) -> TypedPoint2D<i64, U> {
        self.cast().unwrap()
    }
}

impl<T, U> TypedPoint2D<T, U>
where T: Copy + One + Add<Output=T> + Sub<Output=T> + Mul<Output=T> {
    /// Linearly interpolate between this point and another point.
    ///
    /// `t` is expected to be between zero and one.
    #[inline]
    pub fn lerp(&self, other: Self, t: T) -> Self {
        let one_t = T::one() - t;
        point2(
            one_t * self.x + t * other.x,
            one_t * self.y + t * other.y,
        )
    }
}

impl<T: Copy+ApproxEq<T>, U> ApproxEq<TypedPoint2D<T, U>> for TypedPoint2D<T, U> {
    #[inline]
    fn approx_epsilon() -> Self {
        point2(T::approx_epsilon(), T::approx_epsilon())
    }

    #[inline]
    fn approx_eq(&self, other: &Self) -> bool {
        self.x.approx_eq(&other.x) && self.y.approx_eq(&other.y)
    }

    #[inline]
    fn approx_eq_eps(&self, other: &Self, eps: &Self) -> bool {
        self.x.approx_eq_eps(&other.x, &eps.x) && self.y.approx_eq_eps(&other.y, &eps.y)
    }
}

impl<T: Copy, U> Into<[T; 2]> for TypedPoint2D<T, U> {
    fn into(self) -> [T; 2] {
        self.to_array()
    }
}

impl<T: Copy, U> From<[T; 2]> for TypedPoint2D<T, U> {
    fn from(array: [T; 2]) -> Self {
        point2(array[0], array[1])
    }
}


define_matrix! {
    /// A 3d Point tagged with a unit.
    pub struct TypedPoint3D<T, U> {
        pub x: T,
        pub y: T,
        pub z: T,
    }
}

/// Default 3d point type with no unit.
///
/// `Point3D` provides the same methods as `TypedPoint3D`.
pub type Point3D<T> = TypedPoint3D<T, UnknownUnit>;

impl<T: Copy + Zero, U> TypedPoint3D<T, U> {
    /// Constructor, setting all copmonents to zero.
    #[inline]
    pub fn origin() -> Self {
        point3(Zero::zero(), Zero::zero(), Zero::zero())
    }
}

impl<T: Copy + One, U> TypedPoint3D<T, U> {
    #[inline]
    pub fn to_array_4d(&self) -> [T; 4] {
        [self.x, self.y, self.z, One::one()]
    }
}

impl<T, U> TypedPoint3D<T, U>
where T: Copy + One + Add<Output=T> + Sub<Output=T> + Mul<Output=T> {
    /// Linearly interpolate between this point and another point.
    ///
    /// `t` is expected to be between zero and one.
    #[inline]
    pub fn lerp(&self, other: Self, t: T) -> Self {
        let one_t = T::one() - t;
        point3(
            one_t * self.x + t * other.x,
            one_t * self.y + t * other.y,
            one_t * self.z + t * other.z,
        )
    }
}

impl<T: fmt::Debug, U> fmt::Debug for TypedPoint3D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({:?},{:?},{:?})", self.x, self.y, self.z)
    }
}

impl<T: fmt::Display, U> fmt::Display for TypedPoint3D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({},{},{})", self.x, self.y, self.z)
    }
}

impl<T: Copy, U> TypedPoint3D<T, U> {
    /// Constructor taking scalar values directly.
    #[inline]
    pub fn new(x: T, y: T, z: T) -> Self {
        TypedPoint3D { x: x, y: y, z: z, _unit: PhantomData }
    }

    /// Constructor taking properly typed Lengths instead of scalar values.
    #[inline]
    pub fn from_lengths(x: Length<T, U>, y: Length<T, U>, z: Length<T, U>) -> Self {
        point3(x.0, y.0, z.0)
    }

    /// Cast this point into a vector.
    ///
    /// Equivalent to substracting the origin to this point.
    #[inline]
    pub fn to_vector(&self) -> TypedVector3D<T, U> {
        vec3(self.x, self.y, self.z)
    }

    /// Returns a 2d point using this point's x and y coordinates
    #[inline]
    pub fn xy(&self) -> TypedPoint2D<T, U> {
        point2(self.x, self.y)
    }

    /// Returns a 2d point using this point's x and z coordinates
    #[inline]
    pub fn xz(&self) -> TypedPoint2D<T, U> {
        point2(self.x, self.z)
    }

    /// Returns a 2d point using this point's x and z coordinates
    #[inline]
    pub fn yz(&self) -> TypedPoint2D<T, U> {
        point2(self.y, self.z)
    }

    /// Returns self.x as a Length carrying the unit.
    #[inline]
    pub fn x_typed(&self) -> Length<T, U> { Length::new(self.x) }

    /// Returns self.y as a Length carrying the unit.
    #[inline]
    pub fn y_typed(&self) -> Length<T, U> { Length::new(self.y) }

    /// Returns self.z as a Length carrying the unit.
    #[inline]
    pub fn z_typed(&self) -> Length<T, U> { Length::new(self.z) }

    #[inline]
    pub fn to_array(&self) -> [T; 3] { [self.x, self.y, self.z] }

    /// Drop the units, preserving only the numeric value.
    #[inline]
    pub fn to_untyped(&self) -> Point3D<T> {
        point3(self.x, self.y, self.z)
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(p: &Point3D<T>) -> Self {
        point3(p.x, p.y, p.z)
    }

    /// Convert into a 2d point.
    #[inline]
    pub fn to_2d(&self) -> TypedPoint2D<T, U> {
        self.xy()
    }
}

impl<T: Copy + Add<T, Output=T>, U> AddAssign<TypedVector3D<T, U>> for TypedPoint3D<T, U> {
    #[inline]
    fn add_assign(&mut self, other: TypedVector3D<T, U>) {
        *self = *self + other
    }
}

impl<T: Copy + Sub<T, Output=T>, U> SubAssign<TypedVector3D<T, U>> for TypedPoint3D<T, U> {
    #[inline]
    fn sub_assign(&mut self, other: TypedVector3D<T, U>) {
        *self = *self - other
    }
}

impl<T: Copy + Add<T, Output=T>, U> Add<TypedVector3D<T, U>> for TypedPoint3D<T, U> {
    type Output = Self;
    #[inline]
    fn add(self, other: TypedVector3D<T, U>) -> Self {
        point3(self.x + other.x, self.y + other.y, self.z + other.z)
    }
}

impl<T: Copy + Sub<T, Output=T>, U> Sub for TypedPoint3D<T, U> {
    type Output = TypedVector3D<T, U>;
    #[inline]
    fn sub(self, other: Self) -> TypedVector3D<T, U> {
        vec3(self.x - other.x, self.y - other.y, self.z - other.z)
    }
}

impl<T: Copy + Sub<T, Output=T>, U> Sub<TypedVector3D<T, U>> for TypedPoint3D<T, U> {
    type Output = Self;
    #[inline]
    fn sub(self, other: TypedVector3D<T, U>) -> Self {
        point3(self.x - other.x, self.y - other.y, self.z - other.z)
    }
}

impl<T: Copy + Mul<T, Output=T>, U> Mul<T> for TypedPoint3D<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        point3(self.x * scale, self.y * scale, self.z * scale)
    }
}

impl<T: Copy + Div<T, Output=T>, U> Div<T> for TypedPoint3D<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        point3(self.x / scale, self.y / scale, self.z / scale)
    }
}

impl<T: Float, U> TypedPoint3D<T, U> {
    #[inline]
    pub fn min(self, other: Self) -> Self {
         point3(self.x.min(other.x), self.y.min(other.y), self.z.min(other.z))
    }

    #[inline]
    pub fn max(self, other: Self) -> Self {
        point3(self.x.max(other.x), self.y.max(other.y), self.z.max(other.z))
    }
}

impl<T: Round, U> TypedPoint3D<T, U> {
    /// Rounds each component to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    #[inline]
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn round(&self) -> Self {
        point3(self.x.round(), self.y.round(), self.z.round())
    }
}

impl<T: Ceil, U> TypedPoint3D<T, U> {
    /// Rounds each component to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    #[inline]
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn ceil(&self) -> Self {
        point3(self.x.ceil(), self.y.ceil(), self.z.ceil())
    }
}

impl<T: Floor, U> TypedPoint3D<T, U> {
    /// Rounds each component to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    #[inline]
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn floor(&self) -> Self {
        point3(self.x.floor(), self.y.floor(), self.z.floor())
    }
}

impl<T: NumCast + Copy, U> TypedPoint3D<T, U> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using round(), ceil or floor() before casting.
    #[inline]
    pub fn cast<NewT: NumCast + Copy>(&self) -> Option<TypedPoint3D<NewT, U>> {
        match (NumCast::from(self.x),
               NumCast::from(self.y),
               NumCast::from(self.z)) {
            (Some(x), Some(y), Some(z)) => Some(point3(x, y, z)),
            _ => None
        }
    }

    // Convenience functions for common casts

    /// Cast into an `f32` point.
    #[inline]
    pub fn to_f32(&self) -> TypedPoint3D<f32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `f64` point.
    #[inline]
    pub fn to_f64(&self) -> TypedPoint3D<f64, U> {
        self.cast().unwrap()
    }

    /// Cast into an `usize` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_usize(&self) -> TypedPoint3D<usize, U> {
        self.cast().unwrap()
    }

    /// Cast into an `i32` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_i32(&self) -> TypedPoint3D<i32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `i64` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_i64(&self) -> TypedPoint3D<i64, U> {
        self.cast().unwrap()
    }
}

impl<T: Copy+ApproxEq<T>, U> ApproxEq<TypedPoint3D<T, U>> for TypedPoint3D<T, U> {
    #[inline]
    fn approx_epsilon() -> Self {
        point3(T::approx_epsilon(), T::approx_epsilon(), T::approx_epsilon())
    }

    #[inline]
    fn approx_eq(&self, other: &Self) -> bool {
        self.x.approx_eq(&other.x)
            && self.y.approx_eq(&other.y)
            && self.z.approx_eq(&other.z)
    }

    #[inline]
    fn approx_eq_eps(&self, other: &Self, eps: &Self) -> bool {
        self.x.approx_eq_eps(&other.x, &eps.x)
            && self.y.approx_eq_eps(&other.y, &eps.y)
            && self.z.approx_eq_eps(&other.z, &eps.z)
    }
}

impl<T: Copy, U> Into<[T; 3]> for TypedPoint3D<T, U> {
    fn into(self) -> [T; 3] {
        self.to_array()
    }
}

impl<T: Copy, U> From<[T; 3]> for TypedPoint3D<T, U> {
    fn from(array: [T; 3]) -> Self {
        point3(array[0], array[1], array[2])
    }
}


pub fn point2<T: Copy, U>(x: T, y: T) -> TypedPoint2D<T, U> {
    TypedPoint2D::new(x, y)
}

pub fn point3<T: Copy, U>(x: T, y: T, z: T) -> TypedPoint3D<T, U> {
    TypedPoint3D::new(x, y, z)
}

#[cfg(test)]
mod point2d {
    use super::Point2D;

    #[test]
    pub fn test_scalar_mul() {
        let p1: Point2D<f32> = Point2D::new(3.0, 5.0);

        let result = p1 * 5.0;

        assert_eq!(result, Point2D::new(15.0, 25.0));
    }

    #[test]
    pub fn test_min() {
        let p1 = Point2D::new(1.0, 3.0);
        let p2 = Point2D::new(2.0, 2.0);

        let result = p1.min(p2);

        assert_eq!(result, Point2D::new(1.0, 2.0));
    }

    #[test]
    pub fn test_max() {
        let p1 = Point2D::new(1.0, 3.0);
        let p2 = Point2D::new(2.0, 2.0);

        let result = p1.max(p2);

        assert_eq!(result, Point2D::new(2.0, 3.0));
    }
}

#[cfg(test)]
mod typedpoint2d {
    use super::{TypedPoint2D, Point2D, point2};
    use scale::TypedScale;
    use vector::vec2;

    pub enum Mm {}
    pub enum Cm {}

    pub type Point2DMm<T> = TypedPoint2D<T, Mm>;
    pub type Point2DCm<T> = TypedPoint2D<T, Cm>;

    #[test]
    pub fn test_add() {
        let p1 = Point2DMm::new(1.0, 2.0);
        let p2 = vec2(3.0, 4.0);

        let result = p1 + p2;

        assert_eq!(result, Point2DMm::new(4.0, 6.0));
    }

    #[test]
    pub fn test_add_assign() {
        let mut p1 = Point2DMm::new(1.0, 2.0);
        p1 += vec2(3.0, 4.0);

        assert_eq!(p1, Point2DMm::new(4.0, 6.0));
    }

    #[test]
    pub fn test_scalar_mul() {
        let p1 = Point2DMm::new(1.0, 2.0);
        let cm_per_mm: TypedScale<f32, Mm, Cm> = TypedScale::new(0.1);

        let result = p1 * cm_per_mm;

        assert_eq!(result, Point2DCm::new(0.1, 0.2));
    }

    #[test]
    pub fn test_conv_vector() {
        use {Point2D, point2};

        for i in 0..100 {
            // We don't care about these values as long as they are not the same.
            let x = i as f32 *0.012345;
            let y = i as f32 *0.987654;
            let p: Point2D<f32> = point2(x, y);
            assert_eq!(p.to_vector().to_point(), p);
        }
    }

    #[test]
    pub fn test_swizzling() {
        let p: Point2D<i32> = point2(1, 2);
        assert_eq!(p.yx(), point2(2, 1));
    }
}

#[cfg(test)]
mod point3d {
    use super::{Point3D, point2, point3};

    #[test]
    pub fn test_min() {
        let p1 = Point3D::new(1.0, 3.0, 5.0);
        let p2 = Point3D::new(2.0, 2.0, -1.0);

        let result = p1.min(p2);

        assert_eq!(result, Point3D::new(1.0, 2.0, -1.0));
    }

    #[test]
    pub fn test_max() {
        let p1 = Point3D::new(1.0, 3.0, 5.0);
        let p2 = Point3D::new(2.0, 2.0, -1.0);

        let result = p1.max(p2);

        assert_eq!(result, Point3D::new(2.0, 3.0, 5.0));
    }

    #[test]
    pub fn test_conv_vector() {
        use point3;
        for i in 0..100 {
            // We don't care about these values as long as they are not the same.
            let x = i as f32 *0.012345;
            let y = i as f32 *0.987654;
            let z = x * y;
            let p: Point3D<f32> = point3(x, y, z);
            assert_eq!(p.to_vector().to_point(), p);
        }
    }

    #[test]
    pub fn test_swizzling() {
        let p: Point3D<i32> = point3(1, 2, 3);
        assert_eq!(p.xy(), point2(1, 2));
        assert_eq!(p.xz(), point2(1, 3));
        assert_eq!(p.yz(), point2(2, 3));
    }
}
