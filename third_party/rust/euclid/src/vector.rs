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
use point::{TypedPoint2D, TypedPoint3D, point2, point3};
use size::{TypedSize2D, size2};
use scale_factor::ScaleFactor;
use num::*;
use num_traits::{Float, NumCast};
use std::fmt;
use std::ops::{Add, Neg, Mul, Sub, Div, AddAssign, SubAssign, MulAssign, DivAssign};
use std::marker::PhantomData;

define_matrix! {
    /// A 2d Vector tagged with a unit.
    pub struct TypedVector2D<T, U> {
        pub x: T,
        pub y: T,
    }
}

/// Default 2d vector type with no unit.
///
/// `Vector2D` provides the same methods as `TypedVector2D`.
pub type Vector2D<T> = TypedVector2D<T, UnknownUnit>;

impl<T: Copy + Zero, U> TypedVector2D<T, U> {
    /// Constructor, setting all components to zero.
    #[inline]
    pub fn zero() -> Self {
        TypedVector2D::new(Zero::zero(), Zero::zero())
    }

    /// Convert into a 3d vector.
    #[inline]
    pub fn to_3d(&self) -> TypedVector3D<T, U> {
        vec3(self.x, self.y, Zero::zero())
    }
}

impl<T: fmt::Debug, U> fmt::Debug for TypedVector2D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({:?},{:?})", self.x, self.y)
    }
}

impl<T: fmt::Display, U> fmt::Display for TypedVector2D<T, U> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "({},{})", self.x, self.y)
    }
}

impl<T: Copy, U> TypedVector2D<T, U> {
    /// Constructor taking scalar values directly.
    #[inline]
    pub fn new(x: T, y: T) -> Self {
        TypedVector2D { x: x, y: y, _unit: PhantomData }
    }

    /// Constructor taking properly typed Lengths instead of scalar values.
    #[inline]
    pub fn from_lengths(x: Length<T, U>, y: Length<T, U>) -> Self {
        vec2(x.0, y.0)
    }

    /// Create a 3d vector from this one, using the specified z value.
    #[inline]
    pub fn extend(&self, z: T) -> TypedVector3D<T, U> {
        vec3(self.x, self.y, z)
    }

    /// Cast this vector into a point.
    ///
    /// Equivalent to adding this vector to the origin.
    #[inline]
    pub fn to_point(&self) -> TypedPoint2D<T, U> {
        point2(self.x, self.y)
    }

    /// Cast this vector into a size.
    #[inline]
    pub fn to_size(&self) -> TypedSize2D<T, U> {
        size2(self.x, self.y)
    }


    /// Returns self.x as a Length carrying the unit.
    #[inline]
    pub fn x_typed(&self) -> Length<T, U> { Length::new(self.x) }

    /// Returns self.y as a Length carrying the unit.
    #[inline]
    pub fn y_typed(&self) -> Length<T, U> { Length::new(self.y) }

    /// Drop the units, preserving only the numeric value.
    #[inline]
    pub fn to_untyped(&self) -> Vector2D<T> {
        vec2(self.x, self.y)
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(p: &Vector2D<T>) -> Self {
        vec2(p.x, p.y)
    }

    #[inline]
    pub fn to_array(&self) -> [T; 2] {
        [self.x, self.y]
    }
}

impl<T, U> TypedVector2D<T, U>
where T: Copy + Mul<T, Output=T> + Add<T, Output=T> + Sub<T, Output=T> {
    /// Dot product.
    #[inline]
    pub fn dot(self, other: Self) -> T {
        self.x * other.x + self.y * other.y
    }

    /// Returns the norm of the cross product [self.x, self.y, 0] x [other.x, other.y, 0]..
    #[inline]
    pub fn cross(self, other: Self) -> T {
        self.x * other.y - self.y * other.x
    }

    #[inline]
    pub fn normalize(self) -> Self where T: Float + ApproxEq<T> {
        let dot = self.dot(self);
        if dot.approx_eq(&T::zero()) {
            self
        } else {
            self / dot.sqrt()
        }
    }

    #[inline]
    pub fn square_length(&self) -> T {
        self.x * self.x + self.y * self.y
    }

    #[inline]
    pub fn length(&self) -> T where T: Float + ApproxEq<T> {
        self.square_length().sqrt()
    }
}

impl<T, U> TypedVector2D<T, U>
where T: Copy + One + Add<Output=T> + Sub<Output=T> + Mul<Output=T> {
    /// Linearly interpolate between this vector and another vector.
    ///
    /// `t` is expected to be between zero and one.
    #[inline]
    pub fn lerp(&self, other: Self, t: T) -> Self {
        let one_t = T::one() - t;
        (*self) * one_t + other * t
    }
}

impl<T: Copy + Add<T, Output=T>, U> Add for TypedVector2D<T, U> {
    type Output = Self;
    fn add(self, other: Self) -> Self {
        TypedVector2D::new(self.x + other.x, self.y + other.y)
    }
}

impl<T: Copy + Add<T, Output=T>, U> AddAssign for TypedVector2D<T, U> {
    #[inline]
    fn add_assign(&mut self, other: Self) {
        *self = *self + other
    }
}

impl<T: Copy + Sub<T, Output=T>, U> SubAssign<TypedVector2D<T, U>> for TypedVector2D<T, U> {
    #[inline]
    fn sub_assign(&mut self, other: Self) {
        *self = *self - other
    }
}

impl<T: Copy + Sub<T, Output=T>, U> Sub for TypedVector2D<T, U> {
    type Output = Self;
    #[inline]
    fn sub(self, other: Self) -> Self {
        vec2(self.x - other.x, self.y - other.y)
    }
}

impl <T: Copy + Neg<Output=T>, U> Neg for TypedVector2D<T, U> {
    type Output = Self;
    #[inline]
    fn neg(self) -> Self {
        vec2(-self.x, -self.y)
    }
}

impl<T: Float, U> TypedVector2D<T, U> {
    #[inline]
    pub fn min(self, other: Self) -> Self {
         vec2(self.x.min(other.x), self.y.min(other.y))
    }

    #[inline]
    pub fn max(self, other: Self) -> Self {
        vec2(self.x.max(other.x), self.y.max(other.y))
    }
}

impl<T: Copy + Mul<T, Output=T>, U> Mul<T> for TypedVector2D<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        vec2(self.x * scale, self.y * scale)
    }
}

impl<T: Copy + Div<T, Output=T>, U> Div<T> for TypedVector2D<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        vec2(self.x / scale, self.y / scale)
    }
}

impl<T: Copy + Mul<T, Output=T>, U> MulAssign<T> for TypedVector2D<T, U> {
    #[inline]
    fn mul_assign(&mut self, scale: T) {
        *self = *self * scale
    }
}

impl<T: Copy + Div<T, Output=T>, U> DivAssign<T> for TypedVector2D<T, U> {
    #[inline]
    fn div_assign(&mut self, scale: T) {
        *self = *self / scale
    }
}

impl<T: Copy + Mul<T, Output=T>, U1, U2> Mul<ScaleFactor<T, U1, U2>> for TypedVector2D<T, U1> {
    type Output = TypedVector2D<T, U2>;
    #[inline]
    fn mul(self, scale: ScaleFactor<T, U1, U2>) -> TypedVector2D<T, U2> {
        vec2(self.x * scale.get(), self.y * scale.get())
    }
}

impl<T: Copy + Div<T, Output=T>, U1, U2> Div<ScaleFactor<T, U1, U2>> for TypedVector2D<T, U2> {
    type Output = TypedVector2D<T, U1>;
    #[inline]
    fn div(self, scale: ScaleFactor<T, U1, U2>) -> TypedVector2D<T, U1> {
        vec2(self.x / scale.get(), self.y / scale.get())
    }
}

impl<T: Round, U> TypedVector2D<T, U> {
    /// Rounds each component to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    /// For example `{ -0.1, -0.8 }.round() == { 0.0, -1.0 }`.
    #[inline]
    #[must_use]
    pub fn round(&self) -> Self {
        vec2(self.x.round(), self.y.round())
    }
}

impl<T: Ceil, U> TypedVector2D<T, U> {
    /// Rounds each component to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    /// For example `{ -0.1, -0.8 }.ceil() == { 0.0, 0.0 }`.
    #[inline]
    #[must_use]
    pub fn ceil(&self) -> Self {
        vec2(self.x.ceil(), self.y.ceil())
    }
}

impl<T: Floor, U> TypedVector2D<T, U> {
    /// Rounds each component to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    /// For example `{ -0.1, -0.8 }.floor() == { -1.0, -1.0 }`.
    #[inline]
    #[must_use]
    pub fn floor(&self) -> Self {
        vec2(self.x.floor(), self.y.floor())
    }
}

impl<T: NumCast + Copy, U> TypedVector2D<T, U> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating vector to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using `round()`, `ceil()` or `floor()` before casting.
    #[inline]
    pub fn cast<NewT: NumCast + Copy>(&self) -> Option<TypedVector2D<NewT, U>> {
        match (NumCast::from(self.x), NumCast::from(self.y)) {
            (Some(x), Some(y)) => Some(TypedVector2D::new(x, y)),
            _ => None
        }
    }

    // Convenience functions for common casts

    /// Cast into an `f32` vector.
    #[inline]
    pub fn to_f32(&self) -> TypedVector2D<f32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `usize` vector, truncating decimals if any.
    ///
    /// When casting from floating vector vectors, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_usize(&self) -> TypedVector2D<usize, U> {
        self.cast().unwrap()
    }

    /// Cast into an i32 vector, truncating decimals if any.
    ///
    /// When casting from floating vector vectors, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_i32(&self) -> TypedVector2D<i32, U> {
        self.cast().unwrap()
    }

    /// Cast into an i64 vector, truncating decimals if any.
    ///
    /// When casting from floating vector vectors, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_i64(&self) -> TypedVector2D<i64, U> {
        self.cast().unwrap()
    }
}

impl<T: Copy+ApproxEq<T>, U> ApproxEq<TypedVector2D<T, U>> for TypedVector2D<T, U> {
    #[inline]
    fn approx_epsilon() -> Self {
        vec2(T::approx_epsilon(), T::approx_epsilon())
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

impl<T: Copy, U> Into<[T; 2]> for TypedVector2D<T, U> {
    fn into(self) -> [T; 2] {
        self.to_array()
    }
}

impl<T: Copy, U> From<[T; 2]> for TypedVector2D<T, U> {
    fn from(array: [T; 2]) -> Self {
        vec2(array[0], array[1])
    }
}

define_matrix! {
    /// A 3d Vector tagged with a unit.
    pub struct TypedVector3D<T, U> {
        pub x: T,
        pub y: T,
        pub z: T,
    }
}

/// Default 3d vector type with no unit.
///
/// `Vector3D` provides the same methods as `TypedVector3D`.
pub type Vector3D<T> = TypedVector3D<T, UnknownUnit>;

impl<T: Copy + Zero, U> TypedVector3D<T, U> {
    /// Constructor, setting all copmonents to zero.
    #[inline]
    pub fn zero() -> Self {
        vec3(Zero::zero(), Zero::zero(), Zero::zero())
    }

    #[inline]
    pub fn to_array_4d(&self) -> [T; 4] {
        [self.x, self.y, self.z, Zero::zero()]
    }
}

impl<T: fmt::Debug, U> fmt::Debug for TypedVector3D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({:?},{:?},{:?})", self.x, self.y, self.z)
    }
}

impl<T: fmt::Display, U> fmt::Display for TypedVector3D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({},{},{})", self.x, self.y, self.z)
    }
}

impl<T: Copy, U> TypedVector3D<T, U> {
    /// Constructor taking scalar values directly.
    #[inline]
    pub fn new(x: T, y: T, z: T) -> Self {
        TypedVector3D { x: x, y: y, z: z, _unit: PhantomData }
    }

    /// Constructor taking properly typed Lengths instead of scalar values.
    #[inline]
    pub fn from_lengths(x: Length<T, U>, y: Length<T, U>, z: Length<T, U>) -> TypedVector3D<T, U> {
        vec3(x.0, y.0, z.0)
    }

    /// Cast this vector into a point.
    ///
    /// Equivalent to adding this vector to the origin.
    #[inline]
    pub fn to_point(&self) -> TypedPoint3D<T, U> {
        point3(self.x, self.y, self.z)
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
    pub fn to_untyped(&self) -> Vector3D<T> {
        vec3(self.x, self.y, self.z)
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(p: &Vector3D<T>) -> Self {
        vec3(p.x, p.y, p.z)
    }

    /// Convert into a 2d vector.
    #[inline]
    pub fn to_2d(&self) -> TypedVector2D<T, U> {
        vec2(self.x, self.y)
    }
}

impl<T: Mul<T, Output=T> +
        Add<T, Output=T> +
        Sub<T, Output=T> +
        Copy, U> TypedVector3D<T, U> {

    // Dot product.
    #[inline]
    pub fn dot(self, other: Self) -> T {
        self.x * other.x +
        self.y * other.y +
        self.z * other.z
    }

    // Cross product.
    #[inline]
    pub fn cross(self, other: Self) -> Self {
        vec3(
            self.y * other.z - self.z * other.y,
            self.z * other.x - self.x * other.z,
            self.x * other.y - self.y * other.x
        )
    }

    #[inline]
    pub fn normalize(self) -> Self where T: Float + ApproxEq<T> {
        let dot = self.dot(self);
        if dot.approx_eq(&T::zero()) {
            self
        } else {
            self / dot.sqrt()
        }
    }

    #[inline]
    pub fn square_length(&self) -> T {
        self.x * self.x + self.y * self.y + self.z * self.z
    }

    #[inline]
    pub fn length(&self) -> T where T: Float + ApproxEq<T> {
        self.square_length().sqrt()
    }
}

impl<T, U> TypedVector3D<T, U>
where T: Copy + One + Add<Output=T> + Sub<Output=T> + Mul<Output=T> {
    /// Linearly interpolate between this vector and another vector.
    ///
    /// `t` is expected to be between zero and one.
    #[inline]
    pub fn lerp(&self, other: Self, t: T) -> Self {
        let one_t = T::one() - t;
        (*self) * one_t + other * t
    }
}

impl<T: Copy + Add<T, Output=T>, U> Add for TypedVector3D<T, U> {
    type Output = Self;
    #[inline]
    fn add(self, other: Self) -> Self {
        vec3(self.x + other.x, self.y + other.y, self.z + other.z)
    }
}

impl<T: Copy + Sub<T, Output=T>, U> Sub for TypedVector3D<T, U> {
    type Output = Self;
    #[inline]
    fn sub(self, other: Self) -> Self {
        vec3(self.x - other.x, self.y - other.y, self.z - other.z)
    }
}

impl<T: Copy + Add<T, Output=T>, U> AddAssign for TypedVector3D<T, U> {
    #[inline]
    fn add_assign(&mut self, other: Self) {
        *self = *self + other
    }
}

impl<T: Copy + Sub<T, Output=T>, U> SubAssign<TypedVector3D<T, U>> for TypedVector3D<T, U> {
    #[inline]
    fn sub_assign(&mut self, other: Self) {
        *self = *self - other
    }
}

impl <T: Copy + Neg<Output=T>, U> Neg for TypedVector3D<T, U> {
    type Output = Self;
    #[inline]
    fn neg(self) -> Self {
        vec3(-self.x, -self.y, -self.z)
    }
}

impl<T: Copy + Mul<T, Output=T>, U> Mul<T> for TypedVector3D<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        Self::new(self.x * scale, self.y * scale, self.z * scale)
    }
}

impl<T: Copy + Div<T, Output=T>, U> Div<T> for TypedVector3D<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        Self::new(self.x / scale, self.y / scale, self.z / scale)
    }
}

impl<T: Copy + Mul<T, Output=T>, U> MulAssign<T> for TypedVector3D<T, U> {
    #[inline]
    fn mul_assign(&mut self, scale: T) {
        *self = *self * scale
    }
}

impl<T: Copy + Div<T, Output=T>, U> DivAssign<T> for TypedVector3D<T, U> {
    #[inline]
    fn div_assign(&mut self, scale: T) {
        *self = *self / scale
    }
}

impl<T: Float, U> TypedVector3D<T, U> {
    #[inline]
    pub fn min(self, other: Self) -> Self {
         vec3(self.x.min(other.x), self.y.min(other.y), self.z.min(other.z))
    }

    #[inline]
    pub fn max(self, other: Self) -> Self {
        vec3(self.x.max(other.x), self.y.max(other.y), self.z.max(other.z))
    }
}

impl<T: Round, U> TypedVector3D<T, U> {
    /// Rounds each component to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    #[inline]
    #[must_use]
    pub fn round(&self) -> Self {
        vec3(self.x.round(), self.y.round(), self.z.round())
    }
}

impl<T: Ceil, U> TypedVector3D<T, U> {
    /// Rounds each component to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    #[inline]
    #[must_use]
    pub fn ceil(&self) -> Self {
        vec3(self.x.ceil(), self.y.ceil(), self.z.ceil())
    }
}

impl<T: Floor, U> TypedVector3D<T, U> {
    /// Rounds each component to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    #[inline]
    #[must_use]
    pub fn floor(&self) -> Self {
        vec3(self.x.floor(), self.y.floor(), self.z.floor())
    }
}

impl<T: NumCast + Copy, U> TypedVector3D<T, U> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating vector to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using round(), ceil or floor() before casting.
    #[inline]
    pub fn cast<NewT: NumCast + Copy>(&self) -> Option<TypedVector3D<NewT, U>> {
        match (NumCast::from(self.x),
               NumCast::from(self.y),
               NumCast::from(self.z)) {
            (Some(x), Some(y), Some(z)) => Some(vec3(x, y, z)),
            _ => None
        }
    }

    // Convenience functions for common casts

    /// Cast into an `f32` vector.
    #[inline]
    pub fn to_f32(&self) -> TypedVector3D<f32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `usize` vector, truncating decimals if any.
    ///
    /// When casting from floating vector vectors, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_usize(&self) -> TypedVector3D<usize, U> {
        self.cast().unwrap()
    }

    /// Cast into an `i32` vector, truncating decimals if any.
    ///
    /// When casting from floating vector vectors, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_i32(&self) -> TypedVector3D<i32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `i64` vector, truncating decimals if any.
    ///
    /// When casting from floating vector vectors, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    #[inline]
    pub fn to_i64(&self) -> TypedVector3D<i64, U> {
        self.cast().unwrap()
    }
}

impl<T: Copy+ApproxEq<T>, U> ApproxEq<TypedVector3D<T, U>> for TypedVector3D<T, U> {
    #[inline]
    fn approx_epsilon() -> Self {
        vec3(T::approx_epsilon(), T::approx_epsilon(), T::approx_epsilon())
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

impl<T: Copy, U> Into<[T; 3]> for TypedVector3D<T, U> {
    fn into(self) -> [T; 3] {
        self.to_array()
    }
}

impl<T: Copy, U> From<[T; 3]> for TypedVector3D<T, U> {
    fn from(array: [T; 3]) -> Self {
        vec3(array[0], array[1], array[2])
    }
}


/// Convenience constructor.
#[inline]
pub fn vec2<T: Copy, U>(x: T, y: T) -> TypedVector2D<T, U> {
    TypedVector2D::new(x, y)
}

/// Convenience constructor.
#[inline]
pub fn vec3<T: Copy, U>(x: T, y: T, z: T) -> TypedVector3D<T, U> {
    TypedVector3D::new(x, y, z)
}

#[cfg(test)]
mod vector2d {
    use super::{Vector2D, vec2};
    type Vec2 = Vector2D<f32>;

    #[test]
    pub fn test_scalar_mul() {
        let p1: Vec2 = vec2(3.0, 5.0);

        let result = p1 * 5.0;

        assert_eq!(result, Vector2D::new(15.0, 25.0));
    }

    #[test]
    pub fn test_dot() {
        let p1: Vec2 = vec2(2.0, 7.0);
        let p2: Vec2 = vec2(13.0, 11.0);
        assert_eq!(p1.dot(p2), 103.0);
    }

    #[test]
    pub fn test_cross() {
        let p1: Vec2 = vec2(4.0, 7.0);
        let p2: Vec2 = vec2(13.0, 8.0);
        let r = p1.cross(p2);
        assert_eq!(r, -59.0);
    }

    #[test]
    pub fn test_normalize() {
        let p0: Vec2 = Vec2::zero();
        let p1: Vec2 = vec2(4.0, 0.0);
        let p2: Vec2 = vec2(3.0, -4.0);
        assert_eq!(p0.normalize(), p0);
        assert_eq!(p1.normalize(), vec2(1.0, 0.0));
        assert_eq!(p2.normalize(), vec2(0.6, -0.8));
    }

    #[test]
    pub fn test_min() {
        let p1: Vec2 = vec2(1.0, 3.0);
        let p2: Vec2 = vec2(2.0, 2.0);

        let result = p1.min(p2);

        assert_eq!(result, vec2(1.0, 2.0));
    }

    #[test]
    pub fn test_max() {
        let p1: Vec2 = vec2(1.0, 3.0);
        let p2: Vec2 = vec2(2.0, 2.0);

        let result = p1.max(p2);

        assert_eq!(result, vec2(2.0, 3.0));
    }
}

#[cfg(test)]
mod typedvector2d {
    use super::{TypedVector2D, vec2};
    use scale_factor::ScaleFactor;

    pub enum Mm {}
    pub enum Cm {}

    pub type Vector2DMm<T> = TypedVector2D<T, Mm>;
    pub type Vector2DCm<T> = TypedVector2D<T, Cm>;

    #[test]
    pub fn test_add() {
        let p1 = Vector2DMm::new(1.0, 2.0);
        let p2 = Vector2DMm::new(3.0, 4.0);

        let result = p1 + p2;

        assert_eq!(result, vec2(4.0, 6.0));
    }

    #[test]
    pub fn test_add_assign() {
        let mut p1 = Vector2DMm::new(1.0, 2.0);
        p1 += vec2(3.0, 4.0);

        assert_eq!(p1, vec2(4.0, 6.0));
    }

    #[test]
    pub fn test_scalar_mul() {
        let p1 = Vector2DMm::new(1.0, 2.0);
        let cm_per_mm: ScaleFactor<f32, Mm, Cm> = ScaleFactor::new(0.1);

        let result: Vector2DCm<f32> = p1 * cm_per_mm;

        assert_eq!(result, vec2(0.1, 0.2));
    }
}

#[cfg(test)]
mod vector3d {
    use super::{Vector3D, vec3};
    type Vec3 = Vector3D<f32>;

    #[test]
    pub fn test_dot() {
        let p1: Vec3 = vec3(7.0, 21.0, 32.0);
        let p2: Vec3 = vec3(43.0, 5.0, 16.0);
        assert_eq!(p1.dot(p2), 918.0);
    }

    #[test]
    pub fn test_cross() {
        let p1: Vec3 = vec3(4.0, 7.0, 9.0);
        let p2: Vec3 = vec3(13.0, 8.0, 3.0);
        let p3 = p1.cross(p2);
        assert_eq!(p3, vec3(-51.0, 105.0, -59.0));
    }

    #[test]
    pub fn test_normalize() {
        let p0: Vec3 = Vec3::zero();
        let p1: Vec3 = vec3(0.0, -6.0, 0.0);
        let p2: Vec3 = vec3(1.0, 2.0, -2.0);
        assert_eq!(p0.normalize(), p0);
        assert_eq!(p1.normalize(), vec3(0.0, -1.0, 0.0));
        assert_eq!(p2.normalize(), vec3(1.0/3.0, 2.0/3.0, -2.0/3.0));
    }

    #[test]
    pub fn test_min() {
        let p1: Vec3 = vec3(1.0, 3.0, 5.0);
        let p2: Vec3 = vec3(2.0, 2.0, -1.0);

        let result = p1.min(p2);

        assert_eq!(result, vec3(1.0, 2.0, -1.0));
    }

    #[test]
    pub fn test_max() {
        let p1: Vec3 = vec3(1.0, 3.0, 5.0);
        let p2: Vec3 = vec3(2.0, 2.0, -1.0);

        let result = p1.max(p2);

        assert_eq!(result, vec3(2.0, 3.0, 5.0));
    }
}
