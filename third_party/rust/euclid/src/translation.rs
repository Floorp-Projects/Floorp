// Copyright 2018 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use {Vector2D, Point2D, Vector3D, Point3D, Transform2D, Transform3D};
use {Size2D, Rect, vec2, point2, vec3, point3};
use UnknownUnit;
use num::*;
use trig::Trig;
use core::ops::{Add, Sub, Neg, Mul, Div};
use core::marker::PhantomData;
use core::fmt;
use core::cmp::{Eq, PartialEq};
use core::hash::{Hash};
#[cfg(feature = "serde")]
use serde;

/// A 2d transformation from a space to another that can only express translations.
///
/// The main benefit of this type over a Vector2D is the ability to cast
/// between a source and a destination spaces.
///
/// Example:
///
/// ```
/// use euclid::{Translation2D, Point2D, point2};
/// struct ParentSpace;
/// struct ChildSpace;
/// type ScrollOffset = Translation2D<i32, ParentSpace, ChildSpace>;
/// type ParentPoint = Point2D<i32, ParentSpace>;
/// type ChildPoint = Point2D<i32, ChildSpace>;
///
/// let scrolling = ScrollOffset::new(0, 100);
/// let p1: ParentPoint = point2(0, 0);
/// let p2: ChildPoint = scrolling.transform_point(p1);
/// ```
///
#[repr(C)]
#[cfg_attr(feature = "serde", derive(Serialize, Deserialize))]
#[cfg_attr(feature = "serde", serde(bound(serialize = "T: serde::Serialize", deserialize = "T: serde::Deserialize<'de>")))]
pub struct Translation2D<T, Src, Dst> {
    pub x: T,
    pub y: T,
    #[doc(hidden)]
    pub _unit: PhantomData<(Src, Dst)>,
}

impl<T: Copy, Src, Dst> Copy for Translation2D<T, Src, Dst> {}

impl<T: Clone, Src, Dst> Clone for Translation2D<T, Src, Dst> {
    fn clone(&self) -> Self {
        Translation2D {
            x: self.x.clone(),
            y: self.y.clone(),
            _unit: PhantomData,
        }
    }
}

impl<T, Src, Dst> Eq for Translation2D<T, Src, Dst> where T: Eq {}

impl<T, Src, Dst> PartialEq for Translation2D<T, Src, Dst>
    where T: PartialEq
{
    fn eq(&self, other: &Self) -> bool {
        self.x == other.x && self.y == other.y
    }
}

impl<T, Src, Dst> Hash for Translation2D<T, Src, Dst>
    where T: Hash
{
    fn hash<H: ::core::hash::Hasher>(&self, h: &mut H) {
        self.x.hash(h);
        self.y.hash(h);
    }
}

impl<T, Src, Dst> Translation2D<T, Src, Dst> {
    #[inline]
    pub const fn new(x: T, y: T) -> Self {
        Translation2D {
            x,
            y,
            _unit: PhantomData,
        }
    }

    /// No-op, just cast the unit.
    #[inline]
    pub fn transform_size(&self, s: Size2D<T, Src>) -> Size2D<T, Dst> {
        Size2D::new(s.width, s.height)
    }
}

impl<T, Src, Dst> Translation2D<T, Src, Dst>
where
    T : Copy
{
    /// Cast into a 2D vector.
    #[inline]
    pub fn to_vector(&self) -> Vector2D<T, Src> {
        vec2(self.x, self.y)
    }

    /// Cast into an array with x and y.
    #[inline]
    pub fn to_array(&self) -> [T; 2] {
        [self.x, self.y]
    }

    /// Cast into a tuple with x and y.
    #[inline]
    pub fn to_tuple(&self) -> (T, T) {
        (self.x, self.y)
    }

    /// Drop the units, preserving only the numeric value.
    #[inline]
    pub fn to_untyped(&self) -> Translation2D<T, UnknownUnit, UnknownUnit> {
        Translation2D {
            x: self.x,
            y: self.y,
            _unit: PhantomData,
        }
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(t: &Translation2D<T, UnknownUnit, UnknownUnit>) -> Self {
        Translation2D {
            x: t.x,
            y: t.y,
            _unit: PhantomData,
        }
    }
}

impl<T, Src, Dst> Translation2D<T, Src, Dst>
where
    T: Zero
{
    #[inline]
    pub fn identity() -> Self {
        Translation2D::new(T::zero(), T::zero())
    }
}

impl<T, Src, Dst> Translation2D<T, Src, Dst>
where
    T: Zero + PartialEq
{
    #[inline]
    pub fn is_identity(&self) -> bool {
        let _0 = T::zero();
        self.x == _0 && self.y == _0
    }
}

impl<T, Src, Dst> Translation2D<T, Src, Dst>
where
    T: Copy + Add<T, Output = T>
{
    /// Translate a point and cast its unit.
    #[inline]
    pub fn transform_point(&self, p: Point2D<T, Src>) -> Point2D<T, Dst> {
        point2(p.x + self.x, p.y + self.y)
    }

    /// Translate a rectangle and cast its unit.
    #[inline]
    pub fn transform_rect(&self, r: &Rect<T, Src>) -> Rect<T, Dst> {
        Rect {
            origin: self.transform_point(r.origin),
            size: self.transform_size(r.size),
        }
    }
}

impl<T, Src, Dst> Translation2D<T, Src, Dst>
where
    T: Copy + Neg<Output = T>
{
    /// Return the inverse transformation.
    #[inline]
    pub fn inverse(&self) -> Translation2D<T, Dst, Src> {
        Translation2D::new(-self.x, -self.y)
    }
}

impl<T, Src, Dst1, Dst2> Add<Translation2D<T, Dst1, Dst2>>
for Translation2D<T, Src, Dst1>
where
    T: Add<T, Output = T>
{
    type Output = Translation2D<T, Src, Dst2>;
    fn add(self, other: Translation2D<T, Dst1, Dst2>) -> Translation2D<T, Src, Dst2> {
        Translation2D::new(
            self.x + other.x,
            self.y + other.y,
        )
    }
}

impl<T, Src, Dst1, Dst2>
    Sub<Translation2D<T, Dst1, Dst2>>
    for Translation2D<T, Src, Dst2>
where
    T: Sub<T, Output = T>
{
    type Output = Translation2D<T, Src, Dst1>;
    fn sub(self, other: Translation2D<T, Dst1, Dst2>) -> Translation2D<T, Src, Dst1> {
        Translation2D::new(
            self.x - other.x,
            self.y - other.y,
        )
    }
}

impl<T, Src, Dst> Translation2D<T, Src, Dst>
where
    T: Copy
        + Add<T, Output = T>
        + Mul<T, Output = T>
        + Div<T, Output = T>
        + Sub<T, Output = T>
        + Trig
        + PartialOrd
        + One
        + Zero,
{
    /// Returns the matrix representation of this translation.
    #[inline]
    pub fn to_transform(&self) -> Transform2D<T, Src, Dst> {
        Transform2D::create_translation(self.x, self.y)
    }
}

impl<T, Src, Dst> From<Vector2D<T, Src>> for Translation2D<T, Src, Dst>
{
    fn from(v: Vector2D<T, Src>) -> Self {
        Translation2D::new(v.x, v.y)
    }
}

impl<T, Src, Dst> Into<Vector2D<T, Src>> for Translation2D<T, Src, Dst>
{
    fn into(self) -> Vector2D<T, Src> {
        vec2(self.x, self.y)
    }
}

impl<T, Src, Dst> Into<Transform2D<T, Src, Dst>> for Translation2D<T, Src, Dst>
where
    T: Copy
        + Add<T, Output = T>
        + Mul<T, Output = T>
        + Div<T, Output = T>
        + Sub<T, Output = T>
        + Trig
        + PartialOrd
        + One
        + Zero,
{
    fn into(self) -> Transform2D<T, Src, Dst> {
        self.to_transform()
    }
}

impl <T, Src, Dst> Default for Translation2D<T, Src, Dst>
    where T: Zero
{
    fn default() -> Self {
        Self::identity()
    }
}

impl<T: fmt::Debug, Src, Dst> fmt::Debug for Translation2D<T, Src, Dst> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Translation({:?},{:?})", self.x, self.y)
    }
}

impl<T: fmt::Display, Src, Dst> fmt::Display for Translation2D<T, Src, Dst> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({},{})", self.x, self.y)
    }
}


/// A 3d transformation from a space to another that can only express translations.
///
/// The main benefit of this type over a Vector3D is the ability to cast
/// between a source and a destination spaces.
#[repr(C)]
pub struct Translation3D<T, Src, Dst> {
    pub x: T,
    pub y: T,
    pub z: T,
    #[doc(hidden)]
    pub _unit: PhantomData<(Src, Dst)>,
}

impl<T: Copy, Src, Dst> Copy for Translation3D<T, Src, Dst> {}

impl<T: Clone, Src, Dst> Clone for Translation3D<T, Src, Dst> {
    fn clone(&self) -> Self {
        Translation3D {
            x: self.x.clone(),
            y: self.y.clone(),
            z: self.z.clone(),
            _unit: PhantomData,
        }
    }
}

#[cfg(feature = "serde")]
impl<'de, T, Src, Dst> serde::Deserialize<'de> for Translation3D<T, Src, Dst>
    where T: serde::Deserialize<'de>
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where D: serde::Deserializer<'de>
    {
        let (x, y, z) = serde::Deserialize::deserialize(deserializer)?;
        Ok(Translation3D { x, y, z, _unit: PhantomData })
    }
}

#[cfg(feature = "serde")]
impl<T, Src, Dst> serde::Serialize for Translation3D<T, Src, Dst>
    where T: serde::Serialize
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where S: serde::Serializer
    {
        (&self.x, &self.y, &self.z).serialize(serializer)
    }
}

impl<T, Src, Dst> Eq for Translation3D<T, Src, Dst> where T: Eq {}

impl<T, Src, Dst> PartialEq for Translation3D<T, Src, Dst>
    where T: PartialEq
{
    fn eq(&self, other: &Self) -> bool {
        self.x == other.x && self.y == other.y && self.z == other.z
    }
}

impl<T, Src, Dst> Hash for Translation3D<T, Src, Dst>
    where T: Hash
{
    fn hash<H: ::core::hash::Hasher>(&self, h: &mut H) {
        self.x.hash(h);
        self.y.hash(h);
        self.z.hash(h);
    }
}

impl<T, Src, Dst> Translation3D<T, Src, Dst> {
    #[inline]
    pub const fn new(x: T, y: T, z: T) -> Self {
        Translation3D {
            x,
            y,
            z,
            _unit: PhantomData,
        }
    }

    /// No-op, just cast the unit.
    #[inline]
    pub fn transform_size(self, s: Size2D<T, Src>) -> Size2D<T, Dst> {
        Size2D::new(s.width, s.height)
    }
}

impl<T, Src, Dst> Translation3D<T, Src, Dst>
where
    T: Copy
{
    /// Cast into a 3D vector.
    #[inline]
    pub fn to_vector(&self) -> Vector3D<T, Src> {
        vec3(self.x, self.y, self.z)
    }

    /// Cast into an array with x, y and z.
    #[inline]
    pub fn to_array(&self) -> [T; 3] {
        [self.x, self.y, self.z]
    }

    /// Cast into a tuple with x, y and z.
    #[inline]
    pub fn to_tuple(&self) -> (T, T, T) {
        (self.x, self.y, self.z)
    }

    /// Drop the units, preserving only the numeric value.
    #[inline]
    pub fn to_untyped(&self) -> Translation3D<T, UnknownUnit, UnknownUnit> {
        Translation3D {
            x: self.x,
            y: self.y,
            z: self.z,
            _unit: PhantomData,
        }
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(t: &Translation3D<T, UnknownUnit, UnknownUnit>) -> Self {
        Translation3D {
            x: t.x,
            y: t.y,
            z: t.z,
            _unit: PhantomData,
        }
    }
}

impl<T, Src, Dst> Translation3D<T, Src, Dst>
where
    T: Zero
{
    #[inline]
    pub fn identity() -> Self {
        Translation3D::new(T::zero(), T::zero(), T::zero())
    }
}

impl<T, Src, Dst> Translation3D<T, Src, Dst>
where
    T: Zero + PartialEq
{
    #[inline]
    pub fn is_identity(&self) -> bool {
        let _0 = T::zero();
        self.x == _0 && self.y == _0 && self.z == _0
    }
}

impl<T, Src, Dst> Translation3D<T, Src, Dst>
where
    T: Copy + Add<T, Output = T>
{
    /// Translate a point and cast its unit.
    #[inline]
    pub fn transform_point3d(&self, p: &Point3D<T, Src>) -> Point3D<T, Dst> {
        point3(p.x + self.x, p.y + self.y, p.z + self.z)
    }

    /// Translate a point and cast its unit.
    #[inline]
    pub fn transform_point2d(&self, p: &Point2D<T, Src>) -> Point2D<T, Dst> {
        point2(p.x + self.x, p.y + self.y)
    }

    /// Translate a rectangle and cast its unit.
    #[inline]
    pub fn transform_rect(&self, r: &Rect<T, Src>) -> Rect<T, Dst> {
        Rect {
            origin: self.transform_point2d(&r.origin),
            size: self.transform_size(r.size),
        }
    }
}

impl<T, Src, Dst> Translation3D<T, Src, Dst>
where
    T: Copy + Neg<Output = T>
{
    /// Return the inverse transformation.
    #[inline]
    pub fn inverse(&self) -> Translation3D<T, Dst, Src> {
        Translation3D::new(-self.x, -self.y, -self.z)
    }
}

impl<T, Src, Dst1, Dst2> Add<Translation3D<T, Dst1, Dst2>>
for Translation3D<T, Src, Dst1>
where
    T: Add<T, Output = T>
{
    type Output = Translation3D<T, Src, Dst2>;
    fn add(self, other: Translation3D<T, Dst1, Dst2>) -> Translation3D<T, Src, Dst2> {
        Translation3D::new(
            self.x + other.x,
            self.y + other.y,
            self.z + other.z,
        )
    }
}

impl<T, Src, Dst1, Dst2>
    Sub<Translation3D<T, Dst1, Dst2>>
    for Translation3D<T, Src, Dst2>
where
    T: Sub<T, Output = T>
{
    type Output = Translation3D<T, Src, Dst1>;
    fn sub(self, other: Translation3D<T, Dst1, Dst2>) -> Translation3D<T, Src, Dst1> {
        Translation3D::new(
            self.x - other.x,
            self.y - other.y,
            self.z - other.z,
        )
    }
}

impl<T, Src, Dst> Translation3D<T, Src, Dst>
where
    T: Copy +
        Add<T, Output=T> +
        Sub<T, Output=T> +
        Mul<T, Output=T> +
        Div<T, Output=T> +
        Neg<Output=T> +
        PartialOrd +
        Trig +
        One + Zero,
{
    /// Returns the matrix representation of this translation.
    #[inline]
    pub fn to_transform(&self) -> Transform3D<T, Src, Dst> {
        Transform3D::create_translation(self.x, self.y, self.z)
    }
}

impl<T, Src, Dst> From<Vector3D<T, Src>> for Translation3D<T, Src, Dst>
{
    fn from(v: Vector3D<T, Src>) -> Self {
        Translation3D::new(v.x, v.y, v.z)
    }
}

impl<T, Src, Dst> Into<Vector3D<T, Src>> for Translation3D<T, Src, Dst>
{
    fn into(self) -> Vector3D<T, Src> {
        vec3(self.x, self.y, self.z)
    }
}

impl<T, Src, Dst> Into<Transform3D<T, Src, Dst>> for Translation3D<T, Src, Dst>
where
    T: Copy +
        Add<T, Output=T> +
        Sub<T, Output=T> +
        Mul<T, Output=T> +
        Div<T, Output=T> +
        Neg<Output=T> +
        PartialOrd +
        Trig +
        One + Zero,
{
    fn into(self) -> Transform3D<T, Src, Dst> {
        self.to_transform()
    }
}

impl <T, Src, Dst> Default for Translation3D<T, Src, Dst>
    where T: Zero
{
    fn default() -> Self {
        Self::identity()
    }
}

impl<T: fmt::Debug, Src, Dst> fmt::Debug for Translation3D<T, Src, Dst> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Translation({:?},{:?},{:?})", self.x, self.y, self.z)
    }
}

impl<T: fmt::Display, Src, Dst> fmt::Display for Translation3D<T, Src, Dst> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({},{},{})", self.x, self.y, self.z)
    }
}

#[test]
fn simple_translation2d() {
    use rect;

    struct A;
    struct B;

    type Translation = Translation2D<i32, A, B>;
    type SrcRect = Rect<i32, A>;
    type DstRect = Rect<i32, B>;

    let tx = Translation::new(10, -10);
    let r1: SrcRect = rect(10, 20, 30, 40);
    let r2: DstRect = tx.transform_rect(&r1);
    assert_eq!(r2, rect(20, 10, 30, 40));

    let inv_tx = tx.inverse();
    assert_eq!(inv_tx.transform_rect(&r2), r1);

    assert!((tx + inv_tx).is_identity());
}

#[test]
fn simple_translation3d() {
    struct A;
    struct B;

    type Translation = Translation3D<i32, A, B>;
    type SrcPoint = Point3D<i32, A>;
    type DstPoint = Point3D<i32, B>;

    let tx = Translation::new(10, -10, 100);
    let p1: SrcPoint = point3(10, 20, 30);
    let p2: DstPoint = tx.transform_point3d(&p1);
    assert_eq!(p2, point3(20, 10, 130));

    let inv_tx = tx.inverse();
    assert_eq!(inv_tx.transform_point3d(&p2), p1);

    assert!((tx + inv_tx).is_identity());
}
