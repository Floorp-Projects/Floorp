// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "cargo-clippy", allow(just_underscores_and_digits))]

use super::{UnknownUnit, Angle};
#[cfg(feature = "mint")]
use mint;
use num::{One, Zero};
use point::{Point2D, point2};
use vector::{Vector2D, vec2};
use rect::Rect;
use transform3d::Transform3D;
use core::ops::{Add, Mul, Div, Sub, Neg};
use core::marker::PhantomData;
use core::cmp::{Eq, PartialEq};
use core::hash::{Hash};
use approxeq::ApproxEq;
use trig::Trig;
use core::fmt;
use num_traits::NumCast;
#[cfg(feature = "serde")]
use serde;

/// A 2d transform stored as a 3 by 2 matrix in row-major order in memory.
///
/// Transforms can be parametrized over the source and destination units, to describe a
/// transformation from a space to another.
/// For example, `Transform2D<f32, WorldSpace, ScreenSpace>::transform_point4d`
/// takes a `Point2D<f32, WorldSpace>` and returns a `Point2D<f32, ScreenSpace>`.
///
/// Transforms expose a set of convenience methods for pre- and post-transformations.
/// A pre-transformation corresponds to adding an operation that is applied before
/// the rest of the transformation, while a post-transformation adds an operation
/// that is applied after.
///
/// These transforms are for working with _row vectors_, so the matrix math for transforming
/// a vector is `v * T`. If your library is using column vectors, use `row_major` functions when you
/// are asked for `column_major` representations and vice versa.
#[repr(C)]
pub struct Transform2D<T, Src, Dst> {
    pub m11: T, pub m12: T,
    pub m21: T, pub m22: T,
    pub m31: T, pub m32: T,
    #[doc(hidden)]
    pub _unit: PhantomData<(Src, Dst)>,
}

impl<T: Copy, Src, Dst> Copy for Transform2D<T, Src, Dst> {}

impl<T: Clone, Src, Dst> Clone for Transform2D<T, Src, Dst> {
    fn clone(&self) -> Self {
        Transform2D {
            m11: self.m11.clone(),
            m12: self.m12.clone(),
            m21: self.m21.clone(),
            m22: self.m22.clone(),
            m31: self.m31.clone(),
            m32: self.m32.clone(),
            _unit: PhantomData,
        }
    }
}

#[cfg(feature = "serde")]
impl<'de, T, Src, Dst> serde::Deserialize<'de> for Transform2D<T, Src, Dst>
    where T: serde::Deserialize<'de>
{
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
        where D: serde::Deserializer<'de>
    {
        let (
            m11, m12,
            m21, m22,
            m31, m32,
        ) = try!(serde::Deserialize::deserialize(deserializer));
        Ok(Transform2D {
            m11, m12,
            m21, m22,
            m31, m32,
            _unit: PhantomData
        })
    }
}

#[cfg(feature = "serde")]
impl<T, Src, Dst> serde::Serialize for Transform2D<T, Src, Dst>
    where T: serde::Serialize
{
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
        where S: serde::Serializer
    {
        (
            &self.m11, &self.m12,
            &self.m21, &self.m22,
            &self.m31, &self.m32,
        ).serialize(serializer)
    }
}

impl<T, Src, Dst> Eq for Transform2D<T, Src, Dst> where T: Eq {}

impl<T, Src, Dst> PartialEq for Transform2D<T, Src, Dst>
    where T: PartialEq
{
    fn eq(&self, other: &Self) -> bool {
        self.m11 == other.m11 &&
            self.m12 == other.m12 &&
            self.m21 == other.m21 &&
            self.m22 == other.m22 &&
            self.m31 == other.m31 &&
            self.m32 == other.m32
    }
}

impl<T, Src, Dst> Hash for Transform2D<T, Src, Dst>
    where T: Hash
{
    fn hash<H: ::core::hash::Hasher>(&self, h: &mut H) {
        self.m11.hash(h);
        self.m12.hash(h);
        self.m21.hash(h);
        self.m22.hash(h);
        self.m31.hash(h);
        self.m32.hash(h);
    }
}

impl<T: Copy, Src, Dst> Transform2D<T, Src, Dst> {
    /// Create a transform specifying its matrix elements in row-major order.
    ///
    /// Beware: This library is written with the assumption that row vectors
    /// are being used. If your matrices use column vectors (i.e. transforming a vector
    /// is `T * v`), then please use `column_major`
    pub fn row_major(m11: T, m12: T, m21: T, m22: T, m31: T, m32: T) -> Self {
        Transform2D {
            m11, m12,
            m21, m22,
            m31, m32,
            _unit: PhantomData,
        }
    }

    /// Create a transform specifying its matrix elements in column-major order.
    ///
    /// Beware: This library is written with the assumption that row vectors
    /// are being used. If your matrices use column vectors (i.e. transforming a vector
    /// is `T * v`), then please use `row_major`
    pub fn column_major(m11: T, m21: T, m31: T, m12: T, m22: T, m32: T) -> Self {
        Transform2D {
            m11, m12,
            m21, m22,
            m31, m32,
            _unit: PhantomData,
        }
    }

    /// Returns an array containing this transform's terms in row-major order (the order
    /// in which the transform is actually laid out in memory).
    ///
    /// Beware: This library is written with the assumption that row vectors
    /// are being used. If your matrices use column vectors (i.e. transforming a vector
    /// is `T * v`), then please use `to_column_major_array`
    pub fn to_row_major_array(&self) -> [T; 6] {
        [
            self.m11, self.m12,
            self.m21, self.m22,
            self.m31, self.m32
        ]
    }

    /// Returns an array containing this transform's terms in column-major order.
    ///
    /// Beware: This library is written with the assumption that row vectors
    /// are being used. If your matrices use column vectors (i.e. transforming a vector
    /// is `T * v`), then please use `to_row_major_array`
    pub fn to_column_major_array(&self) -> [T; 6] {
        [
            self.m11, self.m21, self.m31,
            self.m12, self.m22, self.m32
        ]
    }

    /// Returns an array containing this transform's 3 rows in (in row-major order)
    /// as arrays.
    ///
    /// This is a convenience method to interface with other libraries like glium.
    ///
    /// Beware: This library is written with the assumption that row vectors
    /// are being used. If your matrices use column vectors (i.e. transforming a vector
    /// is `T * v`), this will return column major arrays.
    pub fn to_row_arrays(&self) -> [[T; 2]; 3] {
        [
            [self.m11, self.m12],
            [self.m21, self.m22],
            [self.m31, self.m32],
        ]
    }

    /// Creates a transform from an array of 6 elements in row-major order.
    ///
    /// Beware: This library is written with the assumption that row vectors
    /// are being used. If your matrices use column vectors (i.e. transforming a vector
    /// is `T * v`), please provide a column major array.
    pub fn from_row_major_array(array: [T; 6]) -> Self {
        Self::row_major(
            array[0], array[1],
            array[2], array[3],
            array[4], array[5],
        )
    }

    /// Creates a transform from 3 rows of 2 elements (row-major order).
    ///
    /// Beware: This library is written with the assumption that row vectors
    /// are being used. If your matrices use column vectors (i.e. transforming a vector
    /// is `T * v`), please provide a column major array.
    pub fn from_row_arrays(array: [[T; 2]; 3]) -> Self {
        Self::row_major(
            array[0][0], array[0][1],
            array[1][0], array[1][1],
            array[2][0], array[2][1],
        )
    }

    /// Drop the units, preserving only the numeric value.
    pub fn to_untyped(&self) -> Transform2D<T, UnknownUnit, UnknownUnit> {
        Transform2D::row_major(
            self.m11, self.m12,
            self.m21, self.m22,
            self.m31, self.m32
        )
    }

    /// Tag a unitless value with units.
    pub fn from_untyped(p: &Transform2D<T, UnknownUnit, UnknownUnit>) -> Self {
        Transform2D::row_major(
            p.m11, p.m12,
            p.m21, p.m22,
            p.m31, p.m32
        )
    }
}

impl<T0: NumCast + Copy, Src, Dst> Transform2D<T0, Src, Dst> {
    /// Cast from one numeric representation to another, preserving the units.
    pub fn cast<T1: NumCast + Copy>(&self) -> Transform2D<T1, Src, Dst> {
        self.try_cast().unwrap()
    }

    /// Fallible cast from one numeric representation to another, preserving the units.
    pub fn try_cast<T1: NumCast + Copy>(&self) -> Option<Transform2D<T1, Src, Dst>> {
        match (NumCast::from(self.m11), NumCast::from(self.m12),
               NumCast::from(self.m21), NumCast::from(self.m22),
               NumCast::from(self.m31), NumCast::from(self.m32)) {
            (Some(m11), Some(m12),
             Some(m21), Some(m22),
             Some(m31), Some(m32)) => {
                Some(Transform2D::row_major(
                    m11, m12,
                    m21, m22,
                    m31, m32
                ))
            },
            _ => None
        }
    }
}

impl<T, Src, Dst> Transform2D<T, Src, Dst>
where T: Copy +
         PartialEq +
         One + Zero {
    pub fn identity() -> Self {
        let (_0, _1) = (Zero::zero(), One::one());
        Transform2D::row_major(
           _1, _0,
           _0, _1,
           _0, _0
        )
    }

    // Intentional not public, because it checks for exact equivalence
    // while most consumers will probably want some sort of approximate
    // equivalence to deal with floating-point errors.
    fn is_identity(&self) -> bool {
        *self == Transform2D::identity()
    }
}

impl<T, Src, Dst> Transform2D<T, Src, Dst>
where T: Copy + Clone +
         Add<T, Output=T> +
         Mul<T, Output=T> +
         Div<T, Output=T> +
         Sub<T, Output=T> +
         Trig +
         PartialOrd +
         One + Zero  {

    /// Returns the multiplication of the two matrices such that mat's transformation
    /// applies after self's transformation.
    ///
    /// Assuming row vectors, this is equivalent to self * mat
    #[must_use]
    pub fn post_transform<NewDst>(&self, mat: &Transform2D<T, Dst, NewDst>) -> Transform2D<T, Src, NewDst> {
        Transform2D::row_major(
            self.m11 * mat.m11 + self.m12 * mat.m21,
            self.m11 * mat.m12 + self.m12 * mat.m22,
            self.m21 * mat.m11 + self.m22 * mat.m21,
            self.m21 * mat.m12 + self.m22 * mat.m22,
            self.m31 * mat.m11 + self.m32 * mat.m21 + mat.m31,
            self.m31 * mat.m12 + self.m32 * mat.m22 + mat.m32,
        )
    }

    /// Returns the multiplication of the two matrices such that mat's transformation
    /// applies before self's transformation.
    ///
    /// Assuming row vectors, this is equivalent to mat * self
    #[inline]
    #[must_use]
    pub fn pre_transform<NewSrc>(&self, mat: &Transform2D<T, NewSrc, Src>) -> Transform2D<T, NewSrc, Dst> {
        mat.post_transform(self)
    }

    /// Returns a translation transform.
    #[inline]
    pub fn create_translation(x: T, y: T) -> Self {
         let (_0, _1): (T, T) = (Zero::zero(), One::one());
         Transform2D::row_major(
            _1, _0,
            _0, _1,
             x,  y
        )
    }

    /// Applies a translation after self's transformation and returns the resulting transform.
    #[inline]
    #[must_use]
    pub fn post_translate(&self, v: Vector2D<T, Dst>) -> Self {
        self.post_transform(&Transform2D::create_translation(v.x, v.y))
    }

    /// Applies a translation before self's transformation and returns the resulting transform.
    #[inline]
    #[must_use]
    pub fn pre_translate(&self, v: Vector2D<T, Src>) -> Self {
        self.pre_transform(&Transform2D::create_translation(v.x, v.y))
    }

    /// Returns a scale transform.
    pub fn create_scale(x: T, y: T) -> Self {
        let _0 = Zero::zero();
        Transform2D::row_major(
             x, _0,
            _0,  y,
            _0, _0
        )
    }

    /// Applies a scale after self's transformation and returns the resulting transform.
    #[inline]
    #[must_use]
    pub fn post_scale(&self, x: T, y: T) -> Self {
        self.post_transform(&Transform2D::create_scale(x, y))
    }

    /// Applies a scale before self's transformation and returns the resulting transform.
    #[inline]
    #[must_use]
    pub fn pre_scale(&self, x: T, y: T) -> Self {
        Transform2D::row_major(
            self.m11 * x, self.m12,
            self.m21,     self.m22 * y,
            self.m31,     self.m32
        )
    }

    /// Returns a rotation transform.
    #[inline]
    pub fn create_rotation(theta: Angle<T>) -> Self {
        let _0 = Zero::zero();
        let cos = theta.get().cos();
        let sin = theta.get().sin();
        Transform2D::row_major(
            cos, _0 - sin,
            sin, cos,
             _0, _0
        )
    }

    /// Applies a rotation after self's transformation and returns the resulting transform.
    #[inline]
    #[must_use]
    pub fn post_rotate(&self, theta: Angle<T>) -> Self {
        self.post_transform(&Transform2D::create_rotation(theta))
    }

    /// Applies a rotation before self's transformation and returns the resulting transform.
    #[inline]
    #[must_use]
    pub fn pre_rotate(&self, theta: Angle<T>) -> Self {
        self.pre_transform(&Transform2D::create_rotation(theta))
    }

    /// Returns the given point transformed by this transform.
    ///
    /// Assuming row vectors, this is equivalent to `p * self`
    #[inline]
    #[must_use]
    pub fn transform_point(&self, point: Point2D<T, Src>) -> Point2D<T, Dst> {
        Point2D::new(
            point.x * self.m11 + point.y * self.m21 + self.m31,
            point.x * self.m12 + point.y * self.m22 + self.m32
        )
    }

    /// Returns the given vector transformed by this matrix.
    ///
    /// Assuming row vectors, this is equivalent to `v * self`
    #[inline]
    #[must_use]
    pub fn transform_vector(&self, vec: Vector2D<T, Src>) -> Vector2D<T, Dst> {
        vec2(vec.x * self.m11 + vec.y * self.m21,
             vec.x * self.m12 + vec.y * self.m22)
    }

    /// Returns a rectangle that encompasses the result of transforming the given rectangle by this
    /// transform.
    #[inline]
    #[must_use]
    pub fn transform_rect(&self, rect: &Rect<T, Src>) -> Rect<T, Dst> {
        let min = rect.min();
        let max = rect.max();
        Rect::from_points(&[
            self.transform_point(min),
            self.transform_point(max),
            self.transform_point(point2(max.x, min.y)),
            self.transform_point(point2(min.x, max.y)),
        ])
    }

    /// Computes and returns the determinant of this transform.
    pub fn determinant(&self) -> T {
        self.m11 * self.m22 - self.m12 * self.m21
    }

    /// Returns the inverse transform if possible.
    #[must_use]
    pub fn inverse(&self) -> Option<Transform2D<T, Dst, Src>> {
        let det = self.determinant();

        let _0: T = Zero::zero();
        let _1: T = One::one();

        if det == _0 {
          return None;
        }

        let inv_det = _1 / det;
        Some(Transform2D::row_major(
            inv_det * self.m22,
            inv_det * (_0 - self.m12),
            inv_det * (_0 - self.m21),
            inv_det * self.m11,
            inv_det * (self.m21 * self.m32 - self.m22 * self.m31),
            inv_det * (self.m31 * self.m12 - self.m11 * self.m32),
        ))
    }

    /// Returns the same transform with a different destination unit.
    #[inline]
    pub fn with_destination<NewDst>(&self) -> Transform2D<T, Src, NewDst> {
        Transform2D::row_major(
            self.m11, self.m12,
            self.m21, self.m22,
            self.m31, self.m32,
        )
    }

    /// Returns the same transform with a different source unit.
    #[inline]
    pub fn with_source<NewSrc>(&self) -> Transform2D<T, NewSrc, Dst> {
        Transform2D::row_major(
            self.m11, self.m12,
            self.m21, self.m22,
            self.m31, self.m32,
        )
    }
}

impl <T, Src, Dst> Transform2D<T, Src, Dst>
where T: Copy + Clone +
         Add<T, Output=T> +
         Sub<T, Output=T> +
         Mul<T, Output=T> +
         Div<T, Output=T> +
         Neg<Output=T> +
         PartialOrd +
         Trig +
         One + Zero {
    /// Create a 3D transform from the current transform
    pub fn to_3d(&self) -> Transform3D<T, Src, Dst> {
        Transform3D::row_major_2d(self.m11, self.m12, self.m21, self.m22, self.m31, self.m32)
    }

}

impl <T, Src, Dst> Default for Transform2D<T, Src, Dst>
    where T: Copy + PartialEq + One + Zero
{
    fn default() -> Self {
        Self::identity()
    }
}

impl<T: ApproxEq<T>, Src, Dst> Transform2D<T, Src, Dst> {
    pub fn approx_eq(&self, other: &Self) -> bool {
        self.m11.approx_eq(&other.m11) && self.m12.approx_eq(&other.m12) &&
        self.m21.approx_eq(&other.m21) && self.m22.approx_eq(&other.m22) &&
        self.m31.approx_eq(&other.m31) && self.m32.approx_eq(&other.m32)
    }
}

impl<T: Copy + fmt::Debug, Src, Dst> fmt::Debug for Transform2D<T, Src, Dst>
where T: Copy + fmt::Debug +
         PartialEq +
         One + Zero {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        if self.is_identity() {
            write!(f, "[I]")
        } else {
            self.to_row_major_array().fmt(f)
        }
    }
}

#[cfg(feature = "mint")]
impl<T, Src, Dst> From<mint::RowMatrix3x2<T>> for Transform2D<T, Src, Dst> {
    fn from(m: mint::RowMatrix3x2<T>) -> Self {
        Transform2D {
            m11: m.x.x, m12: m.x.y,
            m21: m.y.x, m22: m.y.y,
            m31: m.z.x, m32: m.z.y,
            _unit: PhantomData,
        }
    }
}
#[cfg(feature = "mint")]
impl<T, Src, Dst> Into<mint::RowMatrix3x2<T>> for Transform2D<T, Src, Dst> {
    fn into(self) -> mint::RowMatrix3x2<T> {
        mint::RowMatrix3x2 {
            x: mint::Vector2 { x: self.m11, y: self.m12 },
            y: mint::Vector2 { x: self.m21, y: self.m22 },
            z: mint::Vector2 { x: self.m31, y: self.m32 },
        }
    }
}


#[cfg(test)]
mod test {
    use super::*;
    use default;
    use approxeq::ApproxEq;
    #[cfg(feature = "mint")]
    use mint;

    use core::f32::consts::FRAC_PI_2;

    type Mat = default::Transform2D<f32>;

    fn rad(v: f32) -> Angle<f32> { Angle::radians(v) }

    #[test]
    pub fn test_translation() {
        let t1 = Mat::create_translation(1.0, 2.0);
        let t2 = Mat::identity().pre_translate(vec2(1.0, 2.0));
        let t3 = Mat::identity().post_translate(vec2(1.0, 2.0));
        assert_eq!(t1, t2);
        assert_eq!(t1, t3);

        assert_eq!(t1.transform_point(Point2D::new(1.0, 1.0)), Point2D::new(2.0, 3.0));

        assert_eq!(t1.post_transform(&t1), Mat::create_translation(2.0, 4.0));
    }

    #[test]
    pub fn test_rotation() {
        let r1 = Mat::create_rotation(rad(FRAC_PI_2));
        let r2 = Mat::identity().pre_rotate(rad(FRAC_PI_2));
        let r3 = Mat::identity().post_rotate(rad(FRAC_PI_2));
        assert_eq!(r1, r2);
        assert_eq!(r1, r3);

        assert!(r1.transform_point(Point2D::new(1.0, 2.0)).approx_eq(&Point2D::new(2.0, -1.0)));

        assert!(r1.post_transform(&r1).approx_eq(&Mat::create_rotation(rad(FRAC_PI_2*2.0))));
    }

    #[test]
    pub fn test_scale() {
        let s1 = Mat::create_scale(2.0, 3.0);
        let s2 = Mat::identity().pre_scale(2.0, 3.0);
        let s3 = Mat::identity().post_scale(2.0, 3.0);
        assert_eq!(s1, s2);
        assert_eq!(s1, s3);

        assert!(s1.transform_point(Point2D::new(2.0, 2.0)).approx_eq(&Point2D::new(4.0, 6.0)));
    }

    #[test]
    fn test_column_major() {
        assert_eq!(
            Mat::row_major(
                1.0,  2.0,
                3.0,  4.0,
                5.0,  6.0
            ),
            Mat::column_major(
                1.0,  3.0,  5.0,
                2.0,  4.0,  6.0,
            )
        );
    }

    #[test]
    pub fn test_inverse_simple() {
        let m1 = Mat::identity();
        let m2 = m1.inverse().unwrap();
        assert!(m1.approx_eq(&m2));
    }

    #[test]
    pub fn test_inverse_scale() {
        let m1 = Mat::create_scale(1.5, 0.3);
        let m2 = m1.inverse().unwrap();
        assert!(m1.pre_transform(&m2).approx_eq(&Mat::identity()));
    }

    #[test]
    pub fn test_inverse_translate() {
        let m1 = Mat::create_translation(-132.0, 0.3);
        let m2 = m1.inverse().unwrap();
        assert!(m1.pre_transform(&m2).approx_eq(&Mat::identity()));
    }

    #[test]
    fn test_inverse_none() {
        assert!(Mat::create_scale(2.0, 0.0).inverse().is_none());
        assert!(Mat::create_scale(2.0, 2.0).inverse().is_some());
    }

    #[test]
    pub fn test_pre_post() {
        let m1 = default::Transform2D::identity().post_scale(1.0, 2.0).post_translate(vec2(1.0, 2.0));
        let m2 = default::Transform2D::identity().pre_translate(vec2(1.0, 2.0)).pre_scale(1.0, 2.0);
        assert!(m1.approx_eq(&m2));

        let r = Mat::create_rotation(rad(FRAC_PI_2));
        let t = Mat::create_translation(2.0, 3.0);

        let a = Point2D::new(1.0, 1.0);

        assert!(r.post_transform(&t).transform_point(a).approx_eq(&Point2D::new(3.0, 2.0)));
        assert!(t.post_transform(&r).transform_point(a).approx_eq(&Point2D::new(4.0, -3.0)));
        assert!(t.post_transform(&r).transform_point(a).approx_eq(&r.transform_point(t.transform_point(a))));

        assert!(r.pre_transform(&t).transform_point(a).approx_eq(&Point2D::new(4.0, -3.0)));
        assert!(t.pre_transform(&r).transform_point(a).approx_eq(&Point2D::new(3.0, 2.0)));
        assert!(t.pre_transform(&r).transform_point(a).approx_eq(&t.transform_point(r.transform_point(a))));
    }

    #[test]
    fn test_size_of() {
        use core::mem::size_of;
        assert_eq!(size_of::<default::Transform2D<f32>>(), 6*size_of::<f32>());
        assert_eq!(size_of::<default::Transform2D<f64>>(), 6*size_of::<f64>());
    }

    #[test]
    pub fn test_is_identity() {
        let m1 = default::Transform2D::identity();
        assert!(m1.is_identity());
        let m2 = m1.post_translate(vec2(0.1, 0.0));
        assert!(!m2.is_identity());
    }

    #[test]
    pub fn test_transform_vector() {
        // Translation does not apply to vectors.
        let m1 = Mat::create_translation(1.0, 1.0);
        let v1 = vec2(10.0, -10.0);
        assert_eq!(v1, m1.transform_vector(v1));
    }

    #[cfg(feature = "mint")]
    #[test]
    pub fn test_mint() {
        let m1 = Mat::create_rotation(rad(FRAC_PI_2));
        let mm: mint::RowMatrix3x2<_> = m1.into();
        let m2 = Mat::from(mm);

        assert_eq!(m1, m2);
    }
}
