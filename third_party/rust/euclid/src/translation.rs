// Copyright 2018 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use {TypedVector2D, TypedPoint2D, TypedVector3D, TypedPoint3D, TypedTransform2D, TypedTransform3D};
use {TypedSize2D, TypedRect, vec2, point2, vec3, point3};
use num::*;
use trig::Trig;
use core::ops::{Add, Sub, Neg, Mul, Div};
use core::marker::PhantomData;
use core::fmt;

/// A 2d transformation from a space to another that can only express translations.
///
/// The main benefit of this type over a TypedVector2D is the ability to cast
/// between a source and a destination spaces.
///
/// Example:
///
/// ```
/// use euclid::{TypedTranslation2D, TypedPoint2D, point2};
/// struct ParentSpace;
/// struct ChildSpace;
/// type ScrollOffset = TypedTranslation2D<i32, ParentSpace, ChildSpace>;
/// type ParentPoint = TypedPoint2D<i32, ParentSpace>;
/// type ChildPoint = TypedPoint2D<i32, ChildSpace>;
///
/// let scrolling = ScrollOffset::new(0, 100);
/// let p1: ParentPoint = point2(0, 0);
/// let p2: ChildPoint = scrolling.transform_point(&p1);
/// ```
///
#[derive(EuclidMatrix)]
#[repr(C)]
pub struct TypedTranslation2D<T, Src, Dst> {
    pub x: T,
    pub y: T,
    #[doc(hidden)]
    pub _unit: PhantomData<(Src, Dst)>,
}

impl<T, Src, Dst> TypedTranslation2D<T, Src, Dst> {
    #[inline]
    pub fn new(x: T, y: T) -> Self {
        TypedTranslation2D {
            x,
            y,
            _unit: PhantomData,
        }
    }
}

impl<T, Src, Dst> TypedTranslation2D<T, Src, Dst>
where
    T : Copy
{
    #[inline]
    pub fn to_array(&self) -> [T; 2] {
        [self.x, self.y]
    }
}

impl<T, Src, Dst> TypedTranslation2D<T, Src, Dst>
where
    T : Copy + Zero
{
    #[inline]
    pub fn identity() -> Self {
        let _0 = T::zero();
        TypedTranslation2D::new(_0, _0)
    }
}

impl<T, Src, Dst> TypedTranslation2D<T, Src, Dst>
where
    T: Zero + PartialEq
{
    #[inline]
    pub fn is_identity(&self) -> bool {
        self.x == T::zero() && self.y == T::zero()
    }
}

impl<T, Src, Dst> TypedTranslation2D<T, Src, Dst>
where
    T: Copy + Add<T, Output = T>
{
    /// Translate a point and cast its unit.
    #[inline]
    pub fn transform_point(&self, p: &TypedPoint2D<T, Src>) -> TypedPoint2D<T, Dst> {
        point2(p.x + self.x, p.y + self.y)
    }

    /// Translate a rectangle and cast its unit.
    #[inline]
    pub fn transform_rect(&self, r: &TypedRect<T, Src>) -> TypedRect<T, Dst> {
        TypedRect {
            origin: self.transform_point(&r.origin),
            size: self.transform_size(&r.size),
        }
    }

    /// No-op, just cast the unit.
    #[inline]
    pub fn transform_size(&self, s: &TypedSize2D<T, Src>) -> TypedSize2D<T, Dst> {
        TypedSize2D::new(s.width, s.height)
    }

    /// Cast into a 2D vector.
    pub fn to_vector(&self) -> TypedVector2D<T, Src> {
        vec2(self.x, self.y)
    }
}

impl<T, Src, Dst> TypedTranslation2D<T, Src, Dst>
where
    T: Copy + Neg<Output = T>
{
    /// Return the inverse transformation.
    #[inline]
    pub fn inverse(&self) -> TypedTranslation2D<T, Dst, Src> {
        TypedTranslation2D::new(-self.x, -self.y)
    }
}

impl<T, Src, Dst1, Dst2> Add<TypedTranslation2D<T, Dst1, Dst2>>
for TypedTranslation2D<T, Src, Dst1>
where
    T: Copy + Add<T, Output = T>
{
    type Output = TypedTranslation2D<T, Src, Dst2>;
    fn add(self, other: TypedTranslation2D<T, Dst1, Dst2>) -> TypedTranslation2D<T, Src, Dst2> {
        TypedTranslation2D::new(
            self.x + other.x,
            self.y + other.y,
        )
    }
}

impl<T, Src, Dst1, Dst2>
    Sub<TypedTranslation2D<T, Dst1, Dst2>>
    for TypedTranslation2D<T, Src, Dst2>
where
    T: Copy + Sub<T, Output = T>
{
    type Output = TypedTranslation2D<T, Src, Dst1>;
    fn sub(self, other: TypedTranslation2D<T, Dst1, Dst2>) -> TypedTranslation2D<T, Src, Dst1> {
        TypedTranslation2D::new(
            self.x - other.x,
            self.y - other.y,
        )
    }
}

impl<T, Src, Dst> TypedTranslation2D<T, Src, Dst>
where
    T: Copy
        + Clone
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
    pub fn to_transform(&self) -> TypedTransform2D<T, Src, Dst> {
        TypedTransform2D::create_translation(self.x, self.y)
    }
}

impl<T, Src, Dst> From<TypedVector2D<T, Src>> for TypedTranslation2D<T, Src, Dst>
where
    T: Copy
{
    fn from(v: TypedVector2D<T, Src>) -> Self {
        TypedTranslation2D::new(v.x, v.y)
    }
}

impl<T, Src, Dst> Into<TypedVector2D<T, Src>> for TypedTranslation2D<T, Src, Dst>
where
    T: Copy
{
    fn into(self) -> TypedVector2D<T, Src> {
        vec2(self.x, self.y)
    }
}

impl<T, Src, Dst> Into<TypedTransform2D<T, Src, Dst>> for TypedTranslation2D<T, Src, Dst>
where
    T: Copy
        + Clone
        + Add<T, Output = T>
        + Mul<T, Output = T>
        + Div<T, Output = T>
        + Sub<T, Output = T>
        + Trig
        + PartialOrd
        + One
        + Zero,
{
    fn into(self) -> TypedTransform2D<T, Src, Dst> {
        self.to_transform()
    }
}

impl <T, Src, Dst> Default for TypedTranslation2D<T, Src, Dst>
    where T: Copy + Zero
{
    fn default() -> Self {
        Self::identity()
    }
}

impl<T, Src, Dst> fmt::Debug for TypedTranslation2D<T, Src, Dst>
where T: Copy + fmt::Debug {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.to_array().fmt(f)
    }
}



/// A 3d transformation from a space to another that can only express translations.
///
/// The main benefit of this type over a TypedVector3D is the ability to cast
/// between a source and a destination spaces.
#[derive(EuclidMatrix)]
#[repr(C)]
pub struct TypedTranslation3D<T, Src, Dst> {
    pub x: T,
    pub y: T,
    pub z: T,
    #[doc(hidden)]
    pub _unit: PhantomData<(Src, Dst)>,
}

impl<T, Src, Dst> TypedTranslation3D<T, Src, Dst> {
    #[inline]
    pub fn new(x: T, y: T, z: T) -> Self {
        TypedTranslation3D {
            x,
            y,
            z,
            _unit: PhantomData,
        }
    }
}

impl<T, Src, Dst> TypedTranslation3D<T, Src, Dst>
where
    T: Copy
{
    #[inline]
    pub fn to_array(&self) -> [T; 3] {
        [self.x, self.y, self.z]
    }
}

impl<T, Src, Dst> TypedTranslation3D<T, Src, Dst>
where
    T: Copy + Zero
{
    #[inline]
    pub fn identity() -> Self {
        let _0 = T::zero();
        TypedTranslation3D::new(_0, _0, _0)
    }
}

impl<T, Src, Dst> TypedTranslation3D<T, Src, Dst>
where
    T: Zero + PartialEq
{
    #[inline]
    pub fn is_identity(&self) -> bool {
        self.x == T::zero() && self.y == T::zero() && self.z == T::zero()
    }
}

impl<T, Src, Dst> TypedTranslation3D<T, Src, Dst>
where
    T: Copy + Add<T, Output = T>
{
    /// Translate a point and cast its unit.
    #[inline]
    pub fn transform_point3d(&self, p: &TypedPoint3D<T, Src>) -> TypedPoint3D<T, Dst> {
        point3(p.x + self.x, p.y + self.y, p.z + self.z)
    }

    /// Translate a point and cast its unit.
    #[inline]
    pub fn transform_point2d(&self, p: &TypedPoint2D<T, Src>) -> TypedPoint2D<T, Dst> {
        point2(p.x + self.x, p.y + self.y)
    }

    /// Translate a rectangle and cast its unit.
    #[inline]
    pub fn transform_rect(&self, r: &TypedRect<T, Src>) -> TypedRect<T, Dst> {
        TypedRect {
            origin: self.transform_point2d(&r.origin),
            size: self.transform_size(&r.size),
        }
    }

    /// No-op, just cast the unit.
    #[inline]
    pub fn transform_size(&self, s: &TypedSize2D<T, Src>) -> TypedSize2D<T, Dst> {
        TypedSize2D::new(s.width, s.height)
    }

    /// Cast into a 3D vector.
    pub fn to_vector(&self) -> TypedVector3D<T, Src> {
        vec3(self.x, self.y, self.z)
    }
}

impl<T, Src, Dst> TypedTranslation3D<T, Src, Dst>
where
    T: Copy + Neg<Output = T>
{
    /// Return the inverse transformation.
    #[inline]
    pub fn inverse(&self) -> TypedTranslation3D<T, Dst, Src> {
        TypedTranslation3D::new(-self.x, -self.y, -self.z)
    }
}

impl<T, Src, Dst1, Dst2> Add<TypedTranslation3D<T, Dst1, Dst2>>
for TypedTranslation3D<T, Src, Dst1>
where
    T: Copy + Add<T, Output = T>
{
    type Output = TypedTranslation3D<T, Src, Dst2>;
    fn add(self, other: TypedTranslation3D<T, Dst1, Dst2>) -> TypedTranslation3D<T, Src, Dst2> {
        TypedTranslation3D::new(
            self.x + other.x,
            self.y + other.y,
            self.z + other.z,
        )
    }
}

impl<T, Src, Dst1, Dst2>
    Sub<TypedTranslation3D<T, Dst1, Dst2>>
    for TypedTranslation3D<T, Src, Dst2>
where
    T: Copy + Sub<T, Output = T>
{
    type Output = TypedTranslation3D<T, Src, Dst1>;
    fn sub(self, other: TypedTranslation3D<T, Dst1, Dst2>) -> TypedTranslation3D<T, Src, Dst1> {
        TypedTranslation3D::new(
            self.x - other.x,
            self.y - other.y,
            self.z - other.z,
        )
    }
}

impl<T, Src, Dst> TypedTranslation3D<T, Src, Dst>
where
    T: Copy + Clone +
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
    pub fn to_transform(&self) -> TypedTransform3D<T, Src, Dst> {
        TypedTransform3D::create_translation(self.x, self.y, self.z)
    }
}

impl<T, Src, Dst> From<TypedVector3D<T, Src>> for TypedTranslation3D<T, Src, Dst>
where
    T: Copy
{
    fn from(v: TypedVector3D<T, Src>) -> Self {
        TypedTranslation3D::new(v.x, v.y, v.z)
    }
}

impl<T, Src, Dst> Into<TypedVector3D<T, Src>> for TypedTranslation3D<T, Src, Dst>
where
    T: Copy
{
    fn into(self) -> TypedVector3D<T, Src> {
        vec3(self.x, self.y, self.z)
    }
}

impl<T, Src, Dst> Into<TypedTransform3D<T, Src, Dst>> for TypedTranslation3D<T, Src, Dst>
where
    T: Copy + Clone +
        Add<T, Output=T> +
        Sub<T, Output=T> +
        Mul<T, Output=T> +
        Div<T, Output=T> +
        Neg<Output=T> +
        PartialOrd +
        Trig +
        One + Zero,
{
    fn into(self) -> TypedTransform3D<T, Src, Dst> {
        self.to_transform()
    }
}

impl <T, Src, Dst> Default for TypedTranslation3D<T, Src, Dst>
    where T: Copy + Zero
{
    fn default() -> Self {
        Self::identity()
    }
}

impl<T, Src, Dst> fmt::Debug for TypedTranslation3D<T, Src, Dst>
where T: Copy + fmt::Debug {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.to_array().fmt(f)
    }
}

#[test]
fn simple_translation2d() {
    use rect;

    struct A;
    struct B;

    type Translation = TypedTranslation2D<i32, A, B>;
    type SrcRect = TypedRect<i32, A>;
    type DstRect = TypedRect<i32, B>;

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

    type Translation = TypedTranslation3D<i32, A, B>;
    type SrcPoint = TypedPoint3D<i32, A>;
    type DstPoint = TypedPoint3D<i32, B>;

    let tx = Translation::new(10, -10, 100);
    let p1: SrcPoint = point3(10, 20, 30);
    let p2: DstPoint = tx.transform_point3d(&p1);
    assert_eq!(p2, point3(20, 10, 130));

    let inv_tx = tx.inverse();
    assert_eq!(inv_tx.transform_point3d(&p2), p1);

    assert!((tx + inv_tx).is_identity());
}
