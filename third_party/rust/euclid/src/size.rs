// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::UnknownUnit;
#[cfg(feature = "mint")]
use mint;
use length::Length;
use scale::Scale;
use vector::{Vector2D, vec2, BoolVector2D};
use vector::{Vector3D, vec3, BoolVector3D};
use num::*;

use num_traits::{Float, NumCast, Signed};
use core::fmt;
use core::ops::{Add, Div, Mul, Sub};
use core::marker::PhantomData;
use core::cmp::{Eq, PartialEq};
use core::hash::{Hash};
#[cfg(feature = "serde")]
use serde;

/// A 2d size tagged with a unit.
#[repr(C)]
pub struct Size2D<T, U> {
    pub width: T,
    pub height: T,
    #[doc(hidden)]
    pub _unit: PhantomData<U>,
}

impl<T: Copy, U> Copy for Size2D<T, U> {}

impl<T: Clone, U> Clone for Size2D<T, U> {
    fn clone(&self) -> Self {
        Size2D {
            width: self.width.clone(),
            height: self.height.clone(),
            _unit: PhantomData,
        }
    }
}

#[cfg(feature = "serde")]
impl<'de, T, U> serde::Deserialize<'de> for Size2D<T, U>
    where T: serde::Deserialize<'de>
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where D: serde::Deserializer<'de>
    {
        let (width, height) = try!(serde::Deserialize::deserialize(deserializer));
        Ok(Size2D { width, height, _unit: PhantomData })
    }
}

#[cfg(feature = "serde")]
impl<T, U> serde::Serialize for Size2D<T, U>
    where T: serde::Serialize
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where S: serde::Serializer
    {
        (&self.width, &self.height).serialize(serializer)
    }
}

impl<T, U> Eq for Size2D<T, U> where T: Eq {}

impl<T, U> PartialEq for Size2D<T, U>
    where T: PartialEq
{
    fn eq(&self, other: &Self) -> bool {
        self.width == other.width && self.height == other.height
    }
}

impl<T, U> Hash for Size2D<T, U>
    where T: Hash
{
    fn hash<H: ::core::hash::Hasher>(&self, h: &mut H) {
        self.width.hash(h);
        self.height.hash(h);
    }
}

impl<T: fmt::Debug, U> fmt::Debug for Size2D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}×{:?}", self.width, self.height)
    }
}

impl<T: fmt::Display, U> fmt::Display for Size2D<T, U> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "({}x{})", self.width, self.height)
    }
}

impl<T: Default, U> Default for Size2D<T, U> {
    fn default() -> Self {
        Size2D::new(Default::default(), Default::default())
    }
}

impl<T, U> Size2D<T, U> {
    /// Constructor taking scalar values.
    pub fn new(width: T, height: T) -> Self {
        Size2D {
            width,
            height,
            _unit: PhantomData,
        }
    }
}

impl<T: Clone, U> Size2D<T, U> {
    /// Constructor taking scalar strongly typed lengths.
    pub fn from_lengths(width: Length<T, U>, height: Length<T, U>) -> Self {
        Size2D::new(width.get(), height.get())
    }
}

impl<T: Round, U> Size2D<T, U> {
    /// Rounds each component to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn round(&self) -> Self {
        Size2D::new(self.width.round(), self.height.round())
    }
}

impl<T: Ceil, U> Size2D<T, U> {
    /// Rounds each component to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn ceil(&self) -> Self {
        Size2D::new(self.width.ceil(), self.height.ceil())
    }
}

impl<T: Floor, U> Size2D<T, U> {
    /// Rounds each component to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn floor(&self) -> Self {
        Size2D::new(self.width.floor(), self.height.floor())
    }
}

impl<T: Copy + Add<T, Output = T>, U> Add for Size2D<T, U> {
    type Output = Self;
    fn add(self, other: Self) -> Self {
        Size2D::new(self.width + other.width, self.height + other.height)
    }
}

impl<T: Copy + Sub<T, Output = T>, U> Sub for Size2D<T, U> {
    type Output = Self;
    fn sub(self, other: Self) -> Self {
        Size2D::new(self.width - other.width, self.height - other.height)
    }
}

impl<T: Copy + Clone + Mul<T>, U> Size2D<T, U> {
    pub fn area(&self) -> T::Output {
        self.width * self.height
    }
}

impl<T, U> Size2D<T, U>
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

impl<T: Zero + PartialOrd, U> Size2D<T, U> {
    pub fn is_empty_or_negative(&self) -> bool {
        let zero = T::zero();
        self.width <= zero || self.height <= zero
    }
}

impl<T: Zero, U> Size2D<T, U> {
    pub fn zero() -> Self {
        Size2D::new(Zero::zero(), Zero::zero())
    }
}

impl<T: Zero, U> Zero for Size2D<T, U> {
    fn zero() -> Self {
        Size2D::new(Zero::zero(), Zero::zero())
    }
}

impl<T: Copy + Mul<T, Output = T>, U> Mul<T> for Size2D<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        Size2D::new(self.width * scale, self.height * scale)
    }
}

impl<T: Copy + Div<T, Output = T>, U> Div<T> for Size2D<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        Size2D::new(self.width / scale, self.height / scale)
    }
}

impl<T: Copy + Mul<T, Output = T>, U1, U2> Mul<Scale<T, U1, U2>> for Size2D<T, U1> {
    type Output = Size2D<T, U2>;
    #[inline]
    fn mul(self, scale: Scale<T, U1, U2>) -> Size2D<T, U2> {
        Size2D::new(self.width * scale.get(), self.height * scale.get())
    }
}

impl<T: Copy + Div<T, Output = T>, U1, U2> Div<Scale<T, U1, U2>> for Size2D<T, U2> {
    type Output = Size2D<T, U1>;
    #[inline]
    fn div(self, scale: Scale<T, U1, U2>) -> Size2D<T, U1> {
        Size2D::new(self.width / scale.get(), self.height / scale.get())
    }
}

impl<T: Copy, U> Size2D<T, U> {
    /// Returns self.width as a Length carrying the unit.
    #[inline]
    pub fn to_array(&self) -> [T; 2] {
        [self.width, self.height]
    }

    #[inline]
    pub fn to_tuple(&self) -> (T, T) {
        (self.width, self.height)
    }

    #[inline]
    pub fn to_vector(&self) -> Vector2D<T, U> {
        vec2(self.width, self.height)
    }

    /// Drop the units, preserving only the numeric value.
    pub fn to_untyped(&self) -> Size2D<T, UnknownUnit> {
        Size2D::new(self.width, self.height)
    }

    /// Tag a unitless value with units.
    pub fn from_untyped(p: Size2D<T, UnknownUnit>) -> Self {
        Size2D::new(p.width, p.height)
    }
}

impl<T: NumCast + Copy, Unit> Size2D<T, Unit> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using `round()`, `ceil()` or `floor()` before casting.
    pub fn cast<NewT: NumCast + Copy>(&self) -> Size2D<NewT, Unit> {
        self.try_cast().unwrap()
    }

    /// Fallible cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using `round()`, `ceil()` or `floor()` before casting.
    pub fn try_cast<NewT: NumCast + Copy>(&self) -> Option<Size2D<NewT, Unit>> {
        match (NumCast::from(self.width), NumCast::from(self.height)) {
            (Some(w), Some(h)) => Some(Size2D::new(w, h)),
            _ => None,
        }
    }

    // Convenience functions for common casts

    /// Cast into an `f32` size.
    pub fn to_f32(&self) -> Size2D<f32, Unit> {
        self.cast()
    }

    /// Cast into an `f64` size.
    pub fn to_f64(&self) -> Size2D<f64, Unit> {
        self.cast()
    }

    /// Cast into an `uint` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_usize(&self) -> Size2D<usize, Unit> {
        self.cast()
    }

    /// Cast into an `u32` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_u32(&self) -> Size2D<u32, Unit> {
        self.cast()
    }

    /// Cast into an `i32` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i32(&self) -> Size2D<i32, Unit> {
        self.cast()
    }

    /// Cast into an `i64` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i64(&self) -> Size2D<i64, Unit> {
        self.cast()
    }
}

impl<T, U> Size2D<T, U>
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

impl<T: PartialOrd, U> Size2D<T, U> {
    pub fn greater_than(&self, other: Self) -> BoolVector2D {
        BoolVector2D {
            x: self.width > other.width,
            y: self.height > other.height,
        }
    }

    pub fn lower_than(&self, other: Self) -> BoolVector2D {
        BoolVector2D {
            x: self.width < other.width,
            y: self.height < other.height,
        }
    }
}


impl<T: PartialEq, U> Size2D<T, U> {
    pub fn equal(&self, other: Self) -> BoolVector2D {
        BoolVector2D {
            x: self.width == other.width,
            y: self.height == other.height,
        }
    }

    pub fn not_equal(&self, other: Self) -> BoolVector2D {
        BoolVector2D {
            x: self.width != other.width,
            y: self.height != other.height,
        }
    }
}

impl<T: Float, U> Size2D<T, U> {
    #[inline]
    pub fn min(self, other: Self) -> Self {
        size2(
            self.width.min(other.width),
            self.height.min(other.height),
        )
    }

    #[inline]
    pub fn max(self, other: Self) -> Self {
        size2(
            self.width.max(other.width),
            self.height.max(other.height),
        )
    }

    #[inline]
    pub fn clamp(&self, start: Self, end: Self) -> Self {
        self.max(start).min(end)
    }
}


/// Shorthand for `Size2D::new(w, h)`.
pub fn size2<T, U>(w: T, h: T) -> Size2D<T, U> {
    Size2D::new(w, h)
}

#[cfg(feature = "mint")]
impl<T, U> From<mint::Vector2<T>> for Size2D<T, U> {
    fn from(v: mint::Vector2<T>) -> Self {
        Size2D {
            width: v.x,
            height: v.y,
            _unit: PhantomData,
        }
    }
}
#[cfg(feature = "mint")]
impl<T, U> Into<mint::Vector2<T>> for Size2D<T, U> {
    fn into(self) -> mint::Vector2<T> {
        mint::Vector2 {
            x: self.width,
            y: self.height,
        }
    }
}

impl<T, U> From<Vector2D<T, U>> for Size2D<T, U> {
    fn from(v: Vector2D<T, U>) -> Self {
        Size2D {
            width: v.x,
            height: v.y,
            _unit: PhantomData,
        }
    }
}

impl<T: Copy, U> Into<[T; 2]> for Size2D<T, U> {
    fn into(self) -> [T; 2] {
        self.to_array()
    }
}

impl<T: Copy, U> From<[T; 2]> for Size2D<T, U> {
    fn from(array: [T; 2]) -> Self {
        size2(array[0], array[1])
    }
}

impl<T: Copy, U> Into<(T, T)> for Size2D<T, U> {
    fn into(self) -> (T, T) {
        self.to_tuple()
    }
}

impl<T: Copy, U> From<(T, T)> for Size2D<T, U> {
    fn from(tuple: (T, T)) -> Self {
        size2(tuple.0, tuple.1)
    }
}

#[cfg(test)]
mod size2d {
    use default::Size2D;
    #[cfg(feature = "mint")]
    use mint;

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

    #[cfg(feature = "mint")]
    #[test]
    pub fn test_mint() {
        let s1 = Size2D::new(1.0, 2.0);
        let sm: mint::Vector2<_> = s1.into();
        let s2 = Size2D::from(sm);

        assert_eq!(s1, s2);
    }
}

/// A 3d size tagged with a unit.
#[repr(C)]
pub struct Size3D<T, U> {
    pub width: T,
    pub height: T,
    pub depth: T,
    #[doc(hidden)]
    pub _unit: PhantomData<U>,
}

impl<T: Copy, U> Copy for Size3D<T, U> {}

impl<T: Clone, U> Clone for Size3D<T, U> {
    fn clone(&self) -> Self {
        Size3D {
            width: self.width.clone(),
            height: self.height.clone(),
            depth: self.depth.clone(),
            _unit: PhantomData,
        }
    }
}

#[cfg(feature = "serde")]
impl<'de, T, U> serde::Deserialize<'de> for Size3D<T, U>
    where T: serde::Deserialize<'de>
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where D: serde::Deserializer<'de>
    {
        let (width, height, depth) = try!(serde::Deserialize::deserialize(deserializer));
        Ok(Size3D { width, height, depth, _unit: PhantomData })
    }
}

#[cfg(feature = "serde")]
impl<T, U> serde::Serialize for Size3D<T, U>
    where T: serde::Serialize
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where S: serde::Serializer
    {
        (&self.width, &self.height, &self.depth).serialize(serializer)
    }
}

impl<T, U> Eq for Size3D<T, U> where T: Eq {}

impl<T, U> PartialEq for Size3D<T, U>
    where T: PartialEq
{
    fn eq(&self, other: &Self) -> bool {
        self.width == other.width && self.height == other.height && self.depth == other.depth
    }
}

impl<T, U> Hash for Size3D<T, U>
    where T: Hash
{
    fn hash<H: ::core::hash::Hasher>(&self, h: &mut H) {
        self.width.hash(h);
        self.height.hash(h);
        self.depth.hash(h);
    }
}

impl<T: fmt::Debug, U> fmt::Debug for Size3D<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "{:?}×{:?}×{:?}", self.width, self.height, self.depth)
    }
}

impl<T: fmt::Display, U> fmt::Display for Size3D<T, U> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "({}x{}x{})", self.width, self.height, self.depth)
    }
}

impl<T: Default, U> Default for Size3D<T, U> {
    fn default() -> Self {
        Size3D::new(Default::default(), Default::default(), Default::default())
    }
}

impl<T, U> Size3D<T, U> {
    /// Constructor taking scalar values.
    pub fn new(width: T, height: T, depth: T) -> Self {
        Size3D {
            width,
            height,
            depth,
            _unit: PhantomData,
        }
    }
}

impl<T: Clone, U> Size3D<T, U> {
    /// Constructor taking scalar strongly typed lengths.
    pub fn from_lengths(width: Length<T, U>, height: Length<T, U>, depth: Length<T, U>) -> Self {
        Size3D::new(width.get(), height.get(), depth.get())
    }
}

impl<T: Round, U> Size3D<T, U> {
    /// Rounds each component to the nearest integer value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn round(&self) -> Self {
        Size3D::new(self.width.round(), self.height.round(), self.depth.round())
    }
}

impl<T: Ceil, U> Size3D<T, U> {
    /// Rounds each component to the smallest integer equal or greater than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn ceil(&self) -> Self {
        Size3D::new(self.width.ceil(), self.height.ceil(), self.depth.ceil())
    }
}

impl<T: Floor, U> Size3D<T, U> {
    /// Rounds each component to the biggest integer equal or lower than the original value.
    ///
    /// This behavior is preserved for negative values (unlike the basic cast).
    pub fn floor(&self) -> Self {
        Size3D::new(self.width.floor(), self.height.floor(), self.depth.floor())
    }
}

impl<T: Copy + Add<T, Output = T>, U> Add for Size3D<T, U> {
    type Output = Self;
    fn add(self, other: Self) -> Self {
        Size3D::new(self.width + other.width, self.height + other.height, self.depth + other.depth)
    }
}

impl<T: Copy + Sub<T, Output = T>, U> Sub for Size3D<T, U> {
    type Output = Self;
    fn sub(self, other: Self) -> Self {
        Size3D::new(self.width - other.width, self.height - other.height, self.depth - other.depth)
    }
}

impl<T: Copy + Clone + Mul<T, Output=T>, U> Size3D<T, U> {
    pub fn volume(&self) -> T {
        self.width * self.height * self.depth
    }
}

impl<T, U> Size3D<T, U>
where
    T: Copy + One + Add<Output = T> + Sub<Output = T> + Mul<Output = T>,
{
    /// Linearly interpolate between this size and another size.
    ///
    /// `t` is expected to be between zero and one.
    #[inline]
    pub fn lerp(&self, other: Self, t: T) -> Self {
        let one_t = T::one() - t;
        size3(
            one_t * self.width + t * other.width,
            one_t * self.height + t * other.height,
            one_t * self.depth + t * other.depth,
        )
    }
}

impl<T: Zero + PartialOrd, U> Size3D<T, U> {
    pub fn is_empty_or_negative(&self) -> bool {
        let zero = T::zero();
        self.width <= zero || self.height <= zero || self.depth <= zero
    }
}

impl<T: Zero, U> Size3D<T, U> {
    pub fn zero() -> Self {
        Size3D::new(Zero::zero(), Zero::zero(), Zero::zero())
    }
}

impl<T: Zero, U> Zero for Size3D<T, U> {
    fn zero() -> Self {
        Size3D::new(Zero::zero(), Zero::zero(), Zero::zero())
    }
}

impl<T: Copy + Mul<T, Output = T>, U> Mul<T> for Size3D<T, U> {
    type Output = Self;
    #[inline]
    fn mul(self, scale: T) -> Self {
        Size3D::new(self.width * scale, self.height * scale, self.depth * scale)
    }
}

impl<T: Copy + Div<T, Output = T>, U> Div<T> for Size3D<T, U> {
    type Output = Self;
    #[inline]
    fn div(self, scale: T) -> Self {
        Size3D::new(self.width / scale, self.height / scale, self.depth / scale)
    }
}

impl<T: Copy + Mul<T, Output = T>, U1, U2> Mul<Scale<T, U1, U2>> for Size3D<T, U1> {
    type Output = Size3D<T, U2>;
    #[inline]
    fn mul(self, scale: Scale<T, U1, U2>) -> Size3D<T, U2> {
        Size3D::new(self.width * scale.get(), self.height * scale.get(), self.depth * scale.get())
    }
}

impl<T: Copy + Div<T, Output = T>, U1, U2> Div<Scale<T, U1, U2>> for Size3D<T, U2> {
    type Output = Size3D<T, U1>;
    #[inline]
    fn div(self, scale: Scale<T, U1, U2>) -> Size3D<T, U1> {
        Size3D::new(self.width / scale.get(), self.height / scale.get(), self.depth / scale.get())
    }
}

impl<T: Copy, U> Size3D<T, U> {
    /// Returns self.width as a Length carrying the unit.
    #[inline]
    pub fn to_array(&self) -> [T; 3] {
        [self.width, self.height, self.depth]
    }

    #[inline]
    pub fn to_vector(&self) -> Vector3D<T, U> {
        vec3(self.width, self.height, self.depth)
    }

    /// Drop the units, preserving only the numeric value.
    pub fn to_untyped(&self) -> Size3D<T, UnknownUnit> {
        Size3D::new(self.width, self.height, self.depth)
    }

    /// Tag a unitless value with units.
    pub fn from_untyped(p: Size3D<T, UnknownUnit>) -> Self {
        Size3D::new(p.width, p.height, p.depth)
    }
}

impl<T: NumCast + Copy, Unit> Size3D<T, Unit> {
    /// Cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using `round()`, `ceil()` or `floor()` before casting.
    pub fn cast<NewT: NumCast + Copy>(&self) -> Size3D<NewT, Unit> {
        self.try_cast().unwrap()
    }

    /// Fallible cast from one numeric representation to another, preserving the units.
    ///
    /// When casting from floating point to integer coordinates, the decimals are truncated
    /// as one would expect from a simple cast, but this behavior does not always make sense
    /// geometrically. Consider using `round()`, `ceil()` or `floor()` before casting.
    pub fn try_cast<NewT: NumCast + Copy>(&self) -> Option<Size3D<NewT, Unit>> {
        match (NumCast::from(self.width), NumCast::from(self.height), NumCast::from(self.depth)) {
            (Some(w), Some(h), Some(d)) => Some(Size3D::new(w, h, d)),
            _ => None,
        }
    }

    // Convenience functions for common casts

    /// Cast into an `f32` size.
    pub fn to_f32(&self) -> Size3D<f32, Unit> {
        self.cast()
    }

    /// Cast into an `f64` size.
    pub fn to_f64(&self) -> Size3D<f64, Unit> {
        self.cast()
    }

    /// Cast into an `uint` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_usize(&self) -> Size3D<usize, Unit> {
        self.cast()
    }

    /// Cast into an `u32` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_u32(&self) -> Size3D<u32, Unit> {
        self.cast()
    }

    /// Cast into an `i32` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i32(&self) -> Size3D<i32, Unit> {
        self.cast()
    }

    /// Cast into an `i64` size, truncating decimals if any.
    ///
    /// When casting from floating point sizes, it is worth considering whether
    /// to `round()`, `ceil()` or `floor()` before the cast in order to obtain
    /// the desired conversion behavior.
    pub fn to_i64(&self) -> Size3D<i64, Unit> {
        self.cast()
    }
}

impl<T, U> Size3D<T, U>
where
    T: Signed,
{
    pub fn abs(&self) -> Self {
        size3(self.width.abs(), self.height.abs(), self.depth.abs())
    }

    pub fn is_positive(&self) -> bool {
        self.width.is_positive() && self.height.is_positive() && self.depth.is_positive()
    }
}

impl<T: PartialOrd, U> Size3D<T, U> {
    pub fn greater_than(&self, other: Self) -> BoolVector3D {
        BoolVector3D {
            x: self.width > other.width,
            y: self.height > other.height,
            z: self.depth > other.depth,
        }
    }

    pub fn lower_than(&self, other: Self) -> BoolVector3D {
        BoolVector3D {
            x: self.width < other.width,
            y: self.height < other.height,
            z: self.depth < other.depth,
        }
    }
}


impl<T: PartialEq, U> Size3D<T, U> {
    pub fn equal(&self, other: Self) -> BoolVector3D {
        BoolVector3D {
            x: self.width == other.width,
            y: self.height == other.height,
            z: self.depth == other.depth,
        }
    }

    pub fn not_equal(&self, other: Self) -> BoolVector3D {
        BoolVector3D {
            x: self.width != other.width,
            y: self.height != other.height,
            z: self.depth != other.depth,
        }
    }
}

impl<T: Float, U> Size3D<T, U> {
    #[inline]
    pub fn min(self, other: Self) -> Self {
        size3(
            self.width.min(other.width),
            self.height.min(other.height),
            self.depth.min(other.depth),
        )
    }

    #[inline]
    pub fn max(self, other: Self) -> Self {
        size3(
            self.width.max(other.width),
            self.height.max(other.height),
            self.depth.max(other.depth),
        )
    }

    #[inline]
    pub fn clamp(&self, start: Self, end: Self) -> Self {
        self.max(start).min(end)
    }
}


/// Shorthand for `Size3D::new(w, h, d)`.
pub fn size3<T, U>(w: T, h: T, d: T) -> Size3D<T, U> {
    Size3D::new(w, h, d)
}

#[cfg(feature = "mint")]
impl<T, U> From<mint::Vector3<T>> for Size3D<T, U> {
    fn from(v: mint::Vector3<T>) -> Self {
        Size3D {
            width: v.x,
            height: v.y,
            depth: v.z,
            _unit: PhantomData,
        }
    }
}
#[cfg(feature = "mint")]
impl<T, U> Into<mint::Vector3<T>> for Size3D<T, U> {
    fn into(self) -> mint::Vector3<T> {
        mint::Vector3 {
            x: self.width,
            y: self.height,
            z: self.depth,
        }
    }
}
