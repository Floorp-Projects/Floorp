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
use scale_factor::ScaleFactor;
use size::TypedSize2D;
use num::*;
use num_traits::{Float, NumCast};
use std::fmt;
use std::ops::{Add, Neg, Mul, Sub, Div};
use std::marker::PhantomData;

define_matrix! {
    /// A 2d Point tagged with a unit.
    #[derive(RustcDecodable, RustcEncodable)]
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
    pub fn zero() -> TypedPoint2D<T, U> {
        TypedPoint2D::new(Zero::zero(), Zero::zero())
    }

    /// Convert into a 3d point.
    #[inline]
    pub fn to_3d(&self) -> TypedPoint3D<T, U> {
        TypedPoint3D::new(self.x, self.y, Zero::zero())
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
    pub fn new(x: T, y: T) -> TypedPoint2D<T, U> {
        TypedPoint2D { x: x, y: y, _unit: PhantomData }
    }

    /// Constructor taking properly typed Lengths instead of scalar values.
    #[inline]
    pub fn from_lengths(x: Length<T, U>, y: Length<T, U>) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(x.0, y.0)
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
        TypedPoint2D::new(self.x, self.y)
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(p: &Point2D<T>) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(p.x, p.y)
    }

    #[inline]
    pub fn to_array(&self) -> [T; 2] {
        [self.x, self.y]
    }
}

impl<T, U> TypedPoint2D<T, U>
where T: Copy + Mul<T, Output=T> + Add<T, Output=T> + Sub<T, Output=T> {
    /// Dot product.
    #[inline]
    pub fn dot(self, other: TypedPoint2D<T, U>) -> T {
        self.x * other.x + self.y * other.y
    }

    /// Returns the norm of the cross product [self.x, self.y, 0] x [other.x, other.y, 0]..
    #[inline]
    pub fn cross(self, other: TypedPoint2D<T, U>) -> T {
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
}

impl<T: Copy + Add<T, Output=T>, U> Add for TypedPoint2D<T, U> {
    type Output = TypedPoint2D<T, U>;
    fn add(self, other: TypedPoint2D<T, U>) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.x + other.x, self.y + other.y)
    }
}

impl<T: Copy + Add<T, Output=T>, U> Add<TypedSize2D<T, U>> for TypedPoint2D<T, U> {
    type Output = TypedPoint2D<T, U>;
    fn add(self, other: TypedSize2D<T, U>) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.x + other.width, self.y + other.height)
    }
}

impl<T: Copy + Add<T, Output=T>, U> TypedPoint2D<T, U> {
    pub fn add_size(&self, other: &TypedSize2D<T, U>) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.x + other.width, self.y + other.height)
    }
}

impl<T: Copy + Sub<T, Output=T>, U> Sub for TypedPoint2D<T, U> {
    type Output = TypedPoint2D<T, U>;
    fn sub(self, other: TypedPoint2D<T, U>) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.x - other.x, self.y - other.y)
    }
}

impl <T: Copy + Neg<Output=T>, U> Neg for TypedPoint2D<T, U> {
    type Output = TypedPoint2D<T, U>;
    #[inline]
    fn neg(self) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(-self.x, -self.y)
    }
}

impl<T: Float, U> TypedPoint2D<T, U> {
    pub fn min(self, other: TypedPoint2D<T, U>) -> TypedPoint2D<T, U> {
         TypedPoint2D::new(self.x.min(other.x), self.y.min(other.y))
    }

    pub fn max(self, other: TypedPoint2D<T, U>) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.x.max(other.x), self.y.max(other.y))
    }
}

impl<T: Copy + Mul<T, Output=T>, U> Mul<T> for TypedPoint2D<T, U> {
    type Output = TypedPoint2D<T, U>;
    #[inline]
    fn mul(self, scale: T) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.x * scale, self.y * scale)
    }
}

impl<T: Copy + Div<T, Output=T>, U> Div<T> for TypedPoint2D<T, U> {
    type Output = TypedPoint2D<T, U>;
    #[inline]
    fn div(self, scale: T) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.x / scale, self.y / scale)
    }
}

impl<T: Copy + Mul<T, Output=T>, U1, U2> Mul<ScaleFactor<T, U1, U2>> for TypedPoint2D<T, U1> {
    type Output = TypedPoint2D<T, U2>;
    #[inline]
    fn mul(self, scale: ScaleFactor<T, U1, U2>) -> TypedPoint2D<T, U2> {
        TypedPoint2D::new(self.x * scale.get(), self.y * scale.get())
    }
}

impl<T: Copy + Div<T, Output=T>, U1, U2> Div<ScaleFactor<T, U1, U2>> for TypedPoint2D<T, U2> {
    type Output = TypedPoint2D<T, U1>;
    #[inline]
    fn div(self, scale: ScaleFactor<T, U1, U2>) -> TypedPoint2D<T, U1> {
        TypedPoint2D::new(self.x / scale.get(), self.y / scale.get())
    }
}

impl<T: Round, U> TypedPoint2D<T, U> {
    /// Rounds each component to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    /// For example `{ -0.1, -0.8 }.round() == { 0.0, -1.0 }`.
    pub fn round(&self) -> Self {
        TypedPoint2D::new(self.x.round(), self.y.round())
    }
}

impl<T: Ceil, U> TypedPoint2D<T, U> {
    /// Rounds each component to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    /// For example `{ -0.1, -0.8 }.ceil() == { 0.0, 0.0 }`.
    pub fn ceil(&self) -> Self {
        TypedPoint2D::new(self.x.ceil(), self.y.ceil())
    }
}

impl<T: Floor, U> TypedPoint2D<T, U> {
    /// Rounds each component to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    /// For example `{ -0.1, -0.8 }.floor() == { -1.0, -1.0 }`.
    pub fn floor(&self) -> Self {
        TypedPoint2D::new(self.x.floor(), self.y.floor())
    }
}

impl<T: NumCast + Copy, U> TypedPoint2D<T, U> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using `round()`, `ceil()` or `floor()` before casting.
    pub fn cast<NewT: NumCast + Copy>(&self) -> Option<TypedPoint2D<NewT, U>> {
        match (NumCast::from(self.x), NumCast::from(self.y)) {
            (Some(x), Some(y)) => Some(TypedPoint2D::new(x, y)),
            _ => None
        }
    }

    // Convenience functions for common casts

    /// Cast into an `f32` point.
    pub fn to_f32(&self) -> TypedPoint2D<f32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `usize` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_usize(&self) -> TypedPoint2D<usize, U> {
        self.cast().unwrap()
    }

    /// Cast into an i32 point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i32(&self) -> TypedPoint2D<i32, U> {
        self.cast().unwrap()
    }

    /// Cast into an i64 point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i64(&self) -> TypedPoint2D<i64, U> {
        self.cast().unwrap()
    }
}

impl<T: Copy+ApproxEq<T>, U> ApproxEq<TypedPoint2D<T, U>> for TypedPoint2D<T, U> {
    #[inline]
    fn approx_epsilon() -> Self {
        TypedPoint2D::new(T::approx_epsilon(), T::approx_epsilon())
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

define_matrix! {
    /// A 3d Point tagged with a unit.
    #[derive(RustcDecodable, RustcEncodable)]
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
    pub fn zero() -> TypedPoint3D<T, U> {
        TypedPoint3D::new(Zero::zero(), Zero::zero(), Zero::zero())
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
    pub fn new(x: T, y: T, z: T) -> TypedPoint3D<T, U> {
        TypedPoint3D { x: x, y: y, z: z, _unit: PhantomData }
    }

    /// Constructor taking properly typed Lengths instead of scalar values.
    #[inline]
    pub fn from_lengths(x: Length<T, U>, y: Length<T, U>, z: Length<T, U>) -> TypedPoint3D<T, U> {
        TypedPoint3D::new(x.0, y.0, z.0)
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
        TypedPoint3D::new(self.x, self.y, self.z)
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(p: &Point3D<T>) -> TypedPoint3D<T, U> {
        TypedPoint3D::new(p.x, p.y, p.z)
    }

    /// Convert into a 2d point.
    #[inline]
    pub fn to_2d(&self) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.x, self.y)
    }
}

impl<T: Mul<T, Output=T> +
        Add<T, Output=T> +
        Sub<T, Output=T> +
        Copy, U> TypedPoint3D<T, U> {

    // Dot product.
    #[inline]
    pub fn dot(self, other: TypedPoint3D<T, U>) -> T {
        self.x * other.x +
        self.y * other.y +
        self.z * other.z
    }

    // Cross product.
    #[inline]
    pub fn cross(self, other: TypedPoint3D<T, U>) -> TypedPoint3D<T, U> {
        TypedPoint3D::new(self.y * other.z - self.z * other.y,
                          self.z * other.x - self.x * other.z,
                          self.x * other.y - self.y * other.x)
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
}

impl<T: Copy + Add<T, Output=T>, U> Add for TypedPoint3D<T, U> {
    type Output = TypedPoint3D<T, U>;
    fn add(self, other: TypedPoint3D<T, U>) -> TypedPoint3D<T, U> {
        TypedPoint3D::new(self.x + other.x,
                          self.y + other.y,
                          self.z + other.z)
    }
}

impl<T: Copy + Sub<T, Output=T>, U> Sub for TypedPoint3D<T, U> {
    type Output = TypedPoint3D<T, U>;
    fn sub(self, other: TypedPoint3D<T, U>) -> TypedPoint3D<T, U> {
        TypedPoint3D::new(self.x - other.x,
                          self.y - other.y,
                          self.z - other.z)
    }
}

impl <T: Copy + Neg<Output=T>, U> Neg for TypedPoint3D<T, U> {
    type Output = TypedPoint3D<T, U>;
    #[inline]
    fn neg(self) -> TypedPoint3D<T, U> {
        TypedPoint3D::new(-self.x, -self.y, -self.z)
    }
}

impl<T: Copy + Mul<T, Output=T>, U> Mul<T> for TypedPoint3D<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        Self::new(self.x * scale, self.y * scale, self.z * scale)
    }
}

impl<T: Copy + Div<T, Output=T>, U> Div<T> for TypedPoint3D<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        Self::new(self.x / scale, self.y / scale, self.z / scale)
    }
}

impl<T: Float, U> TypedPoint3D<T, U> {
    pub fn min(self, other: TypedPoint3D<T, U>) -> TypedPoint3D<T, U> {
         TypedPoint3D::new(self.x.min(other.x),
                           self.y.min(other.y),
                           self.z.min(other.z))
    }

    pub fn max(self, other: TypedPoint3D<T, U>) -> TypedPoint3D<T, U> {
        TypedPoint3D::new(self.x.max(other.x), self.y.max(other.y),
                     self.z.max(other.z))
    }
}

impl<T: Round, U> TypedPoint3D<T, U> {
    /// Rounds each component to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn round(&self) -> Self {
        TypedPoint3D::new(self.x.round(), self.y.round(), self.z.round())
    }
}

impl<T: Ceil, U> TypedPoint3D<T, U> {
    /// Rounds each component to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn ceil(&self) -> Self {
        TypedPoint3D::new(self.x.ceil(), self.y.ceil(), self.z.ceil())
    }
}

impl<T: Floor, U> TypedPoint3D<T, U> {
    /// Rounds each component to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn floor(&self) -> Self {
        TypedPoint3D::new(self.x.floor(), self.y.floor(), self.z.floor())
    }
}

impl<T: NumCast + Copy, U> TypedPoint3D<T, U> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using round(), ceil or floor() before casting.
    pub fn cast<NewT: NumCast + Copy>(&self) -> Option<TypedPoint3D<NewT, U>> {
        match (NumCast::from(self.x),
               NumCast::from(self.y),
               NumCast::from(self.z)) {
            (Some(x), Some(y), Some(z)) => Some(TypedPoint3D::new(x, y, z)),
            _ => None
        }
    }

    // Convenience functions for common casts

    /// Cast into an `f32` point.
    pub fn to_f32(&self) -> TypedPoint3D<f32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `usize` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_usize(&self) -> TypedPoint3D<usize, U> {
        self.cast().unwrap()
    }

    /// Cast into an `i32` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i32(&self) -> TypedPoint3D<i32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `i64` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i64(&self) -> TypedPoint3D<i64, U> {
        self.cast().unwrap()
    }
}

impl<T: Copy+ApproxEq<T>, U> ApproxEq<TypedPoint3D<T, U>> for TypedPoint3D<T, U> {
    #[inline]
    fn approx_epsilon() -> Self {
        TypedPoint3D::new(T::approx_epsilon(), T::approx_epsilon(), T::approx_epsilon())
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

define_matrix! {
    /// A 4d Point tagged with a unit.
    #[derive(RustcDecodable, RustcEncodable)]
    pub struct TypedPoint4D<T, U> {
        pub x: T,
        pub y: T,
        pub z: T,
        pub w: T,
    }
}

/// Default 4d point with no unit.
///
/// `Point4D` provides the same methods as `TypedPoint4D`.
pub type Point4D<T> = TypedPoint4D<T, UnknownUnit>;

impl<T: Copy + Zero, U> TypedPoint4D<T, U> {
    /// Constructor, setting all copmonents to zero.
    #[inline]
    pub fn zero() -> TypedPoint4D<T, U> {
        TypedPoint4D::new(Zero::zero(), Zero::zero(), Zero::zero(), Zero::zero())
    }
}

impl<T: fmt::Debug, U> fmt::Debug for TypedPoint4D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({:?},{:?},{:?},{:?})", self.x, self.y, self.z, self.w)
    }
}

impl<T: fmt::Display, U> fmt::Display for TypedPoint4D<T, U> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "({},{},{},{})", self.x, self.y, self.z, self.w)
    }
}

impl<T: Copy, U> TypedPoint4D<T, U> {
    /// Constructor taking scalar values directly.
    #[inline]
    pub fn new(x: T, y: T, z: T, w: T) -> TypedPoint4D<T, U> {
        TypedPoint4D { x: x, y: y, z: z, w: w, _unit: PhantomData }
    }

    /// Constructor taking properly typed Lengths instead of scalar values.
    #[inline]
    pub fn from_lengths(x: Length<T, U>,
                        y: Length<T, U>,
                        z: Length<T, U>,
                        w: Length<T, U>) -> TypedPoint4D<T, U> {
        TypedPoint4D::new(x.0, y.0, z.0, w.0)
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

    /// Returns self.w as a Length carrying the unit.
    #[inline]
    pub fn w_typed(&self) -> Length<T, U> { Length::new(self.w) }

    /// Drop the units, preserving only the numeric value.
    #[inline]
    pub fn to_untyped(&self) -> Point4D<T> {
        TypedPoint4D::new(self.x, self.y, self.z, self.w)
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(p: &Point4D<T>) -> TypedPoint4D<T, U> {
        TypedPoint4D::new(p.x, p.y, p.z, p.w)
    }

    #[inline]
    pub fn to_array(&self) -> [T; 4] {
        [self.x, self.y, self.z, self.w]
    }
}

impl<T: Copy + Div<T, Output=T>, U> TypedPoint4D<T, U> {
    /// Convert into a 2d point.
    #[inline]
    pub fn to_2d(self) -> TypedPoint2D<T, U> {
        TypedPoint2D::new(self.x / self.w, self.y / self.w)
    }

    /// Convert into a 3d point.
    #[inline]
    pub fn to_3d(self) -> TypedPoint3D<T, U> {
        TypedPoint3D::new(self.x / self.w, self.y / self.w, self.z / self.w)
    }
}

impl<T: Copy + Add<T, Output=T>, U> Add for TypedPoint4D<T, U> {
    type Output = TypedPoint4D<T, U>;
    fn add(self, other: TypedPoint4D<T, U>) -> TypedPoint4D<T, U> {
        TypedPoint4D::new(self.x + other.x,
                          self.y + other.y,
                          self.z + other.z,
                          self.w + other.w)
    }
}

impl<T: Copy + Sub<T, Output=T>, U> Sub for TypedPoint4D<T, U> {
    type Output = TypedPoint4D<T, U>;
    fn sub(self, other: TypedPoint4D<T, U>) -> TypedPoint4D<T, U> {
        TypedPoint4D::new(self.x - other.x,
                          self.y - other.y,
                          self.z - other.z,
                          self.w - other.w)
    }
}

impl <T: Copy + Neg<Output=T>, U> Neg for TypedPoint4D<T, U> {
    type Output = TypedPoint4D<T, U>;
    #[inline]
    fn neg(self) -> TypedPoint4D<T, U> {
        TypedPoint4D::new(-self.x, -self.y, -self.z, -self.w)
    }
}

impl<T: Float, U> TypedPoint4D<T, U> {
    pub fn min(self, other: TypedPoint4D<T, U>) -> TypedPoint4D<T, U> {
         TypedPoint4D::new(self.x.min(other.x), self.y.min(other.y),
                           self.z.min(other.z), self.w.min(other.w))
    }

    pub fn max(self, other: TypedPoint4D<T, U>) -> TypedPoint4D<T, U> {
        TypedPoint4D::new(self.x.max(other.x), self.y.max(other.y),
                          self.z.max(other.z), self.w.max(other.w))
    }
}

impl<T: Round, U> TypedPoint4D<T, U> {
    /// Rounds each component to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn round(&self) -> Self {
        TypedPoint4D::new(self.x.round(), self.y.round(), self.z.round(), self.w.round())
    }
}

impl<T: Ceil, U> TypedPoint4D<T, U> {
    /// Rounds each component to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn ceil(&self) -> Self {
        TypedPoint4D::new(self.x.ceil(), self.y.ceil(), self.z.ceil(), self.w.ceil())
    }
}

impl<T: Floor, U> TypedPoint4D<T, U> {
    /// Rounds each component to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn floor(&self) -> Self {
        TypedPoint4D::new(self.x.floor(), self.y.floor(), self.z.floor(), self.w.floor())
    }
}

impl<T: NumCast + Copy, U> TypedPoint4D<T, U> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using `round()`, `ceil()` or `floor()` before casting.
    pub fn cast<NewT: NumCast + Copy>(&self) -> Option<TypedPoint4D<NewT, U>> {
        match (NumCast::from(self.x),
               NumCast::from(self.y),
               NumCast::from(self.z),
               NumCast::from(self.w)) {
            (Some(x), Some(y), Some(z), Some(w)) => Some(TypedPoint4D::new(x, y, z, w)),
            _ => None
        }
    }

    // Convenience functions for common casts

    /// Cast into an `f32` point.
    pub fn to_f32(&self) -> TypedPoint4D<f32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `usize` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_usize(&self) -> TypedPoint4D<usize, U> {
        self.cast().unwrap()
    }

    /// Cast into an `i32` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i32(&self) -> TypedPoint4D<i32, U> {
        self.cast().unwrap()
    }

    /// Cast into an `i64` point, truncating decimals if any.
    ///
    /// When casting from floating point points, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i64(&self) -> TypedPoint4D<i64, U> {
        self.cast().unwrap()
    }
}

impl<T: ApproxEq<T>, U> ApproxEq<T> for TypedPoint4D<T, U> {
    fn approx_epsilon() -> T {
        T::approx_epsilon()
    }

    fn approx_eq_eps(&self, other: &Self, approx_epsilon: &T) -> bool {
        self.x.approx_eq_eps(&other.x, approx_epsilon)
        && self.y.approx_eq_eps(&other.y, approx_epsilon)
        && self.z.approx_eq_eps(&other.z, approx_epsilon)
        && self.w.approx_eq_eps(&other.w, approx_epsilon)
    }

    fn approx_eq(&self, other: &Self) -> bool {
        self.approx_eq_eps(&other, &Self::approx_epsilon())
    }
}

pub fn point2<T: Copy, U>(x: T, y: T) -> TypedPoint2D<T, U> {
    TypedPoint2D::new(x, y)
}

pub fn point3<T: Copy, U>(x: T, y: T, z: T) -> TypedPoint3D<T, U> {
    TypedPoint3D::new(x, y, z)
}

pub fn point4<T: Copy, U>(x: T, y: T, z: T, w: T) -> TypedPoint4D<T, U> {
    TypedPoint4D::new(x, y, z, w)
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
    pub fn test_dot() {
        let p1: Point2D<f32> = Point2D::new(2.0, 7.0);
        let p2: Point2D<f32> = Point2D::new(13.0, 11.0);
        assert_eq!(p1.dot(p2), 103.0);
    }

    #[test]
    pub fn test_cross() {
        let p1: Point2D<f32> = Point2D::new(4.0, 7.0);
        let p2: Point2D<f32> = Point2D::new(13.0, 8.0);
        let r = p1.cross(p2);
        assert_eq!(r, -59.0);
    }

    #[test]
    pub fn test_normalize() {
        let p0: Point2D<f32> = Point2D::zero();
        let p1: Point2D<f32> = Point2D::new(4.0, 0.0);
        let p2: Point2D<f32> = Point2D::new(3.0, -4.0);
        assert_eq!(p0.normalize(), p0);
        assert_eq!(p1.normalize(), Point2D::new(1.0, 0.0));
        assert_eq!(p2.normalize(), Point2D::new(0.6, -0.8));
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
    use super::TypedPoint2D;
    use scale_factor::ScaleFactor;

    pub enum Mm {}
    pub enum Cm {}

    pub type Point2DMm<T> = TypedPoint2D<T, Mm>;
    pub type Point2DCm<T> = TypedPoint2D<T, Cm>;

    #[test]
    pub fn test_add() {
        let p1 = Point2DMm::new(1.0, 2.0);
        let p2 = Point2DMm::new(3.0, 4.0);

        let result = p1 + p2;

        assert_eq!(result, Point2DMm::new(4.0, 6.0));
    }

    #[test]
    pub fn test_scalar_mul() {
        let p1 = Point2DMm::new(1.0, 2.0);
        let cm_per_mm: ScaleFactor<f32, Mm, Cm> = ScaleFactor::new(0.1);

        let result = p1 * cm_per_mm;

        assert_eq!(result, Point2DCm::new(0.1, 0.2));
    }
}

#[cfg(test)]
mod point3d {
    use super::Point3D;

    #[test]
    pub fn test_dot() {
        let p1 = Point3D::new(7.0, 21.0, 32.0);
        let p2 = Point3D::new(43.0, 5.0, 16.0);
        assert_eq!(p1.dot(p2), 918.0);
    }

    #[test]
    pub fn test_cross() {
        let p1 = Point3D::new(4.0, 7.0, 9.0);
        let p2 = Point3D::new(13.0, 8.0, 3.0);
        let p3 = p1.cross(p2);
        assert_eq!(p3, Point3D::new(-51.0, 105.0, -59.0));
    }

    #[test]
    pub fn test_normalize() {
        let p0: Point3D<f32> = Point3D::zero();
        let p1: Point3D<f32> = Point3D::new(0.0, -6.0, 0.0);
        let p2: Point3D<f32> = Point3D::new(1.0, 2.0, -2.0);
        assert_eq!(p0.normalize(), p0);
        assert_eq!(p1.normalize(), Point3D::new(0.0, -1.0, 0.0));
        assert_eq!(p2.normalize(), Point3D::new(1.0/3.0, 2.0/3.0, -2.0/3.0));
    }

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
}

#[cfg(test)]
mod point4d {
    use super::Point4D;

    #[test]
    pub fn test_add() {
        let p1 = Point4D::new(7.0, 21.0, 32.0, 1.0);
        let p2 = Point4D::new(43.0, 5.0, 16.0, 2.0);

        let result = p1 + p2;

        assert_eq!(result, Point4D::new(50.0, 26.0, 48.0, 3.0));
    }

    #[test]
    pub fn test_sub() {
        let p1 = Point4D::new(7.0, 21.0, 32.0, 1.0);
        let p2 = Point4D::new(43.0, 5.0, 16.0, 2.0);

        let result = p1 - p2;

        assert_eq!(result, Point4D::new(-36.0, 16.0, 16.0, -1.0));
    }

    #[test]
    pub fn test_min() {
        let p1 = Point4D::new(1.0, 3.0, 5.0, 7.0);
        let p2 = Point4D::new(2.0, 2.0, -1.0, 10.0);

        let result = p1.min(p2);

        assert_eq!(result, Point4D::new(1.0, 2.0, -1.0, 7.0));
    }

    #[test]
    pub fn test_max() {
        let p1 = Point4D::new(1.0, 3.0, 5.0, 7.0);
        let p2 = Point4D::new(2.0, 2.0, -1.0, 10.0);

        let result = p1.max(p2);

        assert_eq!(result, Point4D::new(2.0, 3.0, 5.0, 10.0));
    }
}
