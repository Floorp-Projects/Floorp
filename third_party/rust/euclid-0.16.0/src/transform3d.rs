// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::{UnknownUnit, Angle};
use approxeq::ApproxEq;
use trig::Trig;
use point::{TypedPoint2D, TypedPoint3D, point2, point3};
use vector::{TypedVector2D, TypedVector3D, vec2, vec3};
use rect::TypedRect;
use transform2d::TypedTransform2D;
use scale::TypedScale;
use num::{One, Zero};
use std::ops::{Add, Mul, Sub, Div, Neg};
use std::marker::PhantomData;
use std::fmt;
use num_traits::NumCast;

define_matrix! {
    /// A 3d transform stored as a 4 by 4 matrix in row-major order in memory.
    ///
    /// Transforms can be parametrized over the source and destination units, to describe a
    /// transformation from a space to another.
    /// For example, `TypedTransform3D<f32, WordSpace, ScreenSpace>::transform_point3d`
    /// takes a `TypedPoint3D<f32, WordSpace>` and returns a `TypedPoint3D<f32, ScreenSpace>`.
    ///
    /// Transforms expose a set of convenience methods for pre- and post-transformations.
    /// A pre-transformation corresponds to adding an operation that is applied before
    /// the rest of the transformation, while a post-transformation adds an operation
    /// that is applied after.
    pub struct TypedTransform3D<T, Src, Dst> {
        pub m11: T, pub m12: T, pub m13: T, pub m14: T,
        pub m21: T, pub m22: T, pub m23: T, pub m24: T,
        pub m31: T, pub m32: T, pub m33: T, pub m34: T,
        pub m41: T, pub m42: T, pub m43: T, pub m44: T,
    }
}

/// The default 4d transform type with no units.
pub type Transform3D<T> = TypedTransform3D<T, UnknownUnit, UnknownUnit>;

impl<T, Src, Dst> TypedTransform3D<T, Src, Dst> {
    /// Create a transform specifying its components in row-major order.
    ///
    /// For example, the translation terms m41, m42, m43 on the last row with the
    /// row-major convention) are the 13rd, 14th and 15th parameters.
    #[inline]
    pub fn row_major(
            m11: T, m12: T, m13: T, m14: T,
            m21: T, m22: T, m23: T, m24: T,
            m31: T, m32: T, m33: T, m34: T,
            m41: T, m42: T, m43: T, m44: T)
         -> Self {
        TypedTransform3D {
            m11: m11, m12: m12, m13: m13, m14: m14,
            m21: m21, m22: m22, m23: m23, m24: m24,
            m31: m31, m32: m32, m33: m33, m34: m34,
            m41: m41, m42: m42, m43: m43, m44: m44,
            _unit: PhantomData,
        }
    }

    /// Create a transform specifying its components in column-major order.
    ///
    /// For example, the translation terms m41, m42, m43 on the last column with the
    /// column-major convention) are the 4th, 8th and 12nd parameters.
    #[inline]
    pub fn column_major(
            m11: T, m21: T, m31: T, m41: T,
            m12: T, m22: T, m32: T, m42: T,
            m13: T, m23: T, m33: T, m43: T,
            m14: T, m24: T, m34: T, m44: T)
         -> Self {
        TypedTransform3D {
            m11: m11, m12: m12, m13: m13, m14: m14,
            m21: m21, m22: m22, m23: m23, m24: m24,
            m31: m31, m32: m32, m33: m33, m34: m34,
            m41: m41, m42: m42, m43: m43, m44: m44,
            _unit: PhantomData,
        }
    }
}

impl <T, Src, Dst> TypedTransform3D<T, Src, Dst>
where T: Copy + Clone +
         PartialEq +
         One + Zero {
    #[inline]
    pub fn identity() -> Self {
        let (_0, _1): (T, T) = (Zero::zero(), One::one());
        TypedTransform3D::row_major(
            _1, _0, _0, _0,
            _0, _1, _0, _0,
            _0, _0, _1, _0,
            _0, _0, _0, _1
        )
    }

    // Intentional not public, because it checks for exact equivalence
    // while most consumers will probably want some sort of approximate
    // equivalence to deal with floating-point errors.
    #[inline]
    fn is_identity(&self) -> bool {
        *self == TypedTransform3D::identity()
    }
}

impl <T, Src, Dst> TypedTransform3D<T, Src, Dst>
where T: Copy + Clone +
         Add<T, Output=T> +
         Sub<T, Output=T> +
         Mul<T, Output=T> +
         Div<T, Output=T> +
         Neg<Output=T> +
         ApproxEq<T> +
         PartialOrd +
         Trig +
         One + Zero {

    /// Create a 4 by 4 transform representing a 2d transformation, specifying its components
    /// in row-major order.
    #[inline]
    pub fn row_major_2d(m11: T, m12: T, m21: T, m22: T, m41: T, m42: T) -> Self {
        let (_0, _1): (T, T) = (Zero::zero(), One::one());
        TypedTransform3D::row_major(
            m11, m12, _0, _0,
            m21, m22, _0, _0,
             _0,  _0, _1, _0,
            m41, m42, _0, _1
       )
    }

    /// Create an orthogonal projection transform.
    pub fn ortho(left: T, right: T,
                 bottom: T, top: T,
                 near: T, far: T) -> Self {
        let tx = -((right + left) / (right - left));
        let ty = -((top + bottom) / (top - bottom));
        let tz = -((far + near) / (far - near));

        let (_0, _1): (T, T) = (Zero::zero(), One::one());
        let _2 = _1 + _1;
        TypedTransform3D::row_major(
            _2 / (right - left), _0                 , _0                , _0,
            _0                 , _2 / (top - bottom), _0                , _0,
            _0                 , _0                 , -_2 / (far - near), _0,
            tx                 , ty                 , tz                , _1
        )
    }

    /// Returns true if this transform can be represented with a TypedTransform2D.
    ///
    /// See https://drafts.csswg.org/css-transforms/#2d-transform
    #[inline]
    pub fn is_2d(&self) -> bool {
        let (_0, _1): (T, T) = (Zero::zero(), One::one());
        self.m31 == _0 && self.m32 == _0 &&
        self.m13 == _0 && self.m23 == _0 &&
        self.m43 == _0 && self.m14 == _0 &&
        self.m24 == _0 && self.m34 == _0 &&
        self.m33 == _1 && self.m44 == _1
    }

    /// Create a 2D transform picking the relevent terms from this transform.
    ///
    /// This method assumes that self represents a 2d transformation, callers
    /// should check that self.is_2d() returns true beforehand.
    pub fn to_2d(&self) -> TypedTransform2D<T, Src, Dst> {
        TypedTransform2D::row_major(
            self.m11, self.m12,
            self.m21, self.m22,
            self.m41, self.m42
        )
    }

    /// Check whether shapes on the XY plane with Z pointing towards the
    /// screen transformed by this matrix would be facing back.
    pub fn is_backface_visible(&self) -> bool {
        // inverse().m33 < 0;
        let det = self.determinant();
        let m33 = self.m12 * self.m24 * self.m41 - self.m14 * self.m22 * self.m41 +
                  self.m14 * self.m21 * self.m42 - self.m11 * self.m24 * self.m42 -
                  self.m12 * self.m21 * self.m44 + self.m11 * self.m22 * self.m44;
        let _0: T = Zero::zero();
        (m33 * det) < _0
    }

    pub fn approx_eq(&self, other: &Self) -> bool {
        self.m11.approx_eq(&other.m11) && self.m12.approx_eq(&other.m12) &&
        self.m13.approx_eq(&other.m13) && self.m14.approx_eq(&other.m14) &&
        self.m21.approx_eq(&other.m21) && self.m22.approx_eq(&other.m22) &&
        self.m23.approx_eq(&other.m23) && self.m24.approx_eq(&other.m24) &&
        self.m31.approx_eq(&other.m31) && self.m32.approx_eq(&other.m32) &&
        self.m33.approx_eq(&other.m33) && self.m34.approx_eq(&other.m34) &&
        self.m41.approx_eq(&other.m41) && self.m42.approx_eq(&other.m42) &&
        self.m43.approx_eq(&other.m43) && self.m44.approx_eq(&other.m44)
    }

    /// Returns the same transform with a different destination unit.
    #[inline]
    pub fn with_destination<NewDst>(&self) -> TypedTransform3D<T, Src, NewDst> {
        TypedTransform3D::row_major(
            self.m11, self.m12, self.m13, self.m14,
            self.m21, self.m22, self.m23, self.m24,
            self.m31, self.m32, self.m33, self.m34,
            self.m41, self.m42, self.m43, self.m44,
        )
    }

    /// Returns the same transform with a different source unit.
    #[inline]
    pub fn with_source<NewSrc>(&self) -> TypedTransform3D<T, NewSrc, Dst> {
        TypedTransform3D::row_major(
            self.m11, self.m12, self.m13, self.m14,
            self.m21, self.m22, self.m23, self.m24,
            self.m31, self.m32, self.m33, self.m34,
            self.m41, self.m42, self.m43, self.m44,
        )
    }

    /// Drop the units, preserving only the numeric value.
    #[inline]
    pub fn to_untyped(&self) -> Transform3D<T> {
        Transform3D::row_major(
            self.m11, self.m12, self.m13, self.m14,
            self.m21, self.m22, self.m23, self.m24,
            self.m31, self.m32, self.m33, self.m34,
            self.m41, self.m42, self.m43, self.m44,
        )
    }

    /// Tag a unitless value with units.
    #[inline]
    pub fn from_untyped(m: &Transform3D<T>) -> Self {
        TypedTransform3D::row_major(
            m.m11, m.m12, m.m13, m.m14,
            m.m21, m.m22, m.m23, m.m24,
            m.m31, m.m32, m.m33, m.m34,
            m.m41, m.m42, m.m43, m.m44,
        )
    }

    /// Returns the multiplication of the two matrices such that mat's transformation
    /// applies after self's transformation.
    pub fn post_mul<NewDst>(&self, mat: &TypedTransform3D<T, Dst, NewDst>) -> TypedTransform3D<T, Src, NewDst> {
        TypedTransform3D::row_major(
            self.m11 * mat.m11  +  self.m12 * mat.m21  +  self.m13 * mat.m31  +  self.m14 * mat.m41,
            self.m11 * mat.m12  +  self.m12 * mat.m22  +  self.m13 * mat.m32  +  self.m14 * mat.m42,
            self.m11 * mat.m13  +  self.m12 * mat.m23  +  self.m13 * mat.m33  +  self.m14 * mat.m43,
            self.m11 * mat.m14  +  self.m12 * mat.m24  +  self.m13 * mat.m34  +  self.m14 * mat.m44,
            self.m21 * mat.m11  +  self.m22 * mat.m21  +  self.m23 * mat.m31  +  self.m24 * mat.m41,
            self.m21 * mat.m12  +  self.m22 * mat.m22  +  self.m23 * mat.m32  +  self.m24 * mat.m42,
            self.m21 * mat.m13  +  self.m22 * mat.m23  +  self.m23 * mat.m33  +  self.m24 * mat.m43,
            self.m21 * mat.m14  +  self.m22 * mat.m24  +  self.m23 * mat.m34  +  self.m24 * mat.m44,
            self.m31 * mat.m11  +  self.m32 * mat.m21  +  self.m33 * mat.m31  +  self.m34 * mat.m41,
            self.m31 * mat.m12  +  self.m32 * mat.m22  +  self.m33 * mat.m32  +  self.m34 * mat.m42,
            self.m31 * mat.m13  +  self.m32 * mat.m23  +  self.m33 * mat.m33  +  self.m34 * mat.m43,
            self.m31 * mat.m14  +  self.m32 * mat.m24  +  self.m33 * mat.m34  +  self.m34 * mat.m44,
            self.m41 * mat.m11  +  self.m42 * mat.m21  +  self.m43 * mat.m31  +  self.m44 * mat.m41,
            self.m41 * mat.m12  +  self.m42 * mat.m22  +  self.m43 * mat.m32  +  self.m44 * mat.m42,
            self.m41 * mat.m13  +  self.m42 * mat.m23  +  self.m43 * mat.m33  +  self.m44 * mat.m43,
            self.m41 * mat.m14  +  self.m42 * mat.m24  +  self.m43 * mat.m34  +  self.m44 * mat.m44,
        )
    }

    /// Returns the multiplication of the two matrices such that mat's transformation
    /// applies before self's transformation.
    pub fn pre_mul<NewSrc>(&self, mat: &TypedTransform3D<T, NewSrc, Src>) -> TypedTransform3D<T, NewSrc, Dst> {
        mat.post_mul(self)
    }

    /// Returns the inverse transform if possible.
    pub fn inverse(&self) -> Option<TypedTransform3D<T, Dst, Src>> {
        let det = self.determinant();

        if det == Zero::zero() {
            return None;
        }

        // todo(gw): this could be made faster by special casing
        // for simpler transform types.
        let m = TypedTransform3D::row_major(
            self.m23*self.m34*self.m42 - self.m24*self.m33*self.m42 +
            self.m24*self.m32*self.m43 - self.m22*self.m34*self.m43 -
            self.m23*self.m32*self.m44 + self.m22*self.m33*self.m44,

            self.m14*self.m33*self.m42 - self.m13*self.m34*self.m42 -
            self.m14*self.m32*self.m43 + self.m12*self.m34*self.m43 +
            self.m13*self.m32*self.m44 - self.m12*self.m33*self.m44,

            self.m13*self.m24*self.m42 - self.m14*self.m23*self.m42 +
            self.m14*self.m22*self.m43 - self.m12*self.m24*self.m43 -
            self.m13*self.m22*self.m44 + self.m12*self.m23*self.m44,

            self.m14*self.m23*self.m32 - self.m13*self.m24*self.m32 -
            self.m14*self.m22*self.m33 + self.m12*self.m24*self.m33 +
            self.m13*self.m22*self.m34 - self.m12*self.m23*self.m34,

            self.m24*self.m33*self.m41 - self.m23*self.m34*self.m41 -
            self.m24*self.m31*self.m43 + self.m21*self.m34*self.m43 +
            self.m23*self.m31*self.m44 - self.m21*self.m33*self.m44,

            self.m13*self.m34*self.m41 - self.m14*self.m33*self.m41 +
            self.m14*self.m31*self.m43 - self.m11*self.m34*self.m43 -
            self.m13*self.m31*self.m44 + self.m11*self.m33*self.m44,

            self.m14*self.m23*self.m41 - self.m13*self.m24*self.m41 -
            self.m14*self.m21*self.m43 + self.m11*self.m24*self.m43 +
            self.m13*self.m21*self.m44 - self.m11*self.m23*self.m44,

            self.m13*self.m24*self.m31 - self.m14*self.m23*self.m31 +
            self.m14*self.m21*self.m33 - self.m11*self.m24*self.m33 -
            self.m13*self.m21*self.m34 + self.m11*self.m23*self.m34,

            self.m22*self.m34*self.m41 - self.m24*self.m32*self.m41 +
            self.m24*self.m31*self.m42 - self.m21*self.m34*self.m42 -
            self.m22*self.m31*self.m44 + self.m21*self.m32*self.m44,

            self.m14*self.m32*self.m41 - self.m12*self.m34*self.m41 -
            self.m14*self.m31*self.m42 + self.m11*self.m34*self.m42 +
            self.m12*self.m31*self.m44 - self.m11*self.m32*self.m44,

            self.m12*self.m24*self.m41 - self.m14*self.m22*self.m41 +
            self.m14*self.m21*self.m42 - self.m11*self.m24*self.m42 -
            self.m12*self.m21*self.m44 + self.m11*self.m22*self.m44,

            self.m14*self.m22*self.m31 - self.m12*self.m24*self.m31 -
            self.m14*self.m21*self.m32 + self.m11*self.m24*self.m32 +
            self.m12*self.m21*self.m34 - self.m11*self.m22*self.m34,

            self.m23*self.m32*self.m41 - self.m22*self.m33*self.m41 -
            self.m23*self.m31*self.m42 + self.m21*self.m33*self.m42 +
            self.m22*self.m31*self.m43 - self.m21*self.m32*self.m43,

            self.m12*self.m33*self.m41 - self.m13*self.m32*self.m41 +
            self.m13*self.m31*self.m42 - self.m11*self.m33*self.m42 -
            self.m12*self.m31*self.m43 + self.m11*self.m32*self.m43,

            self.m13*self.m22*self.m41 - self.m12*self.m23*self.m41 -
            self.m13*self.m21*self.m42 + self.m11*self.m23*self.m42 +
            self.m12*self.m21*self.m43 - self.m11*self.m22*self.m43,

            self.m12*self.m23*self.m31 - self.m13*self.m22*self.m31 +
            self.m13*self.m21*self.m32 - self.m11*self.m23*self.m32 -
            self.m12*self.m21*self.m33 + self.m11*self.m22*self.m33
        );

        let _1: T = One::one();
        Some(m.mul_s(_1 / det))
    }

    /// Compute the determinant of the transform.
    pub fn determinant(&self) -> T {
        self.m14 * self.m23 * self.m32 * self.m41 -
        self.m13 * self.m24 * self.m32 * self.m41 -
        self.m14 * self.m22 * self.m33 * self.m41 +
        self.m12 * self.m24 * self.m33 * self.m41 +
        self.m13 * self.m22 * self.m34 * self.m41 -
        self.m12 * self.m23 * self.m34 * self.m41 -
        self.m14 * self.m23 * self.m31 * self.m42 +
        self.m13 * self.m24 * self.m31 * self.m42 +
        self.m14 * self.m21 * self.m33 * self.m42 -
        self.m11 * self.m24 * self.m33 * self.m42 -
        self.m13 * self.m21 * self.m34 * self.m42 +
        self.m11 * self.m23 * self.m34 * self.m42 +
        self.m14 * self.m22 * self.m31 * self.m43 -
        self.m12 * self.m24 * self.m31 * self.m43 -
        self.m14 * self.m21 * self.m32 * self.m43 +
        self.m11 * self.m24 * self.m32 * self.m43 +
        self.m12 * self.m21 * self.m34 * self.m43 -
        self.m11 * self.m22 * self.m34 * self.m43 -
        self.m13 * self.m22 * self.m31 * self.m44 +
        self.m12 * self.m23 * self.m31 * self.m44 +
        self.m13 * self.m21 * self.m32 * self.m44 -
        self.m11 * self.m23 * self.m32 * self.m44 -
        self.m12 * self.m21 * self.m33 * self.m44 +
        self.m11 * self.m22 * self.m33 * self.m44
    }

    /// Multiplies all of the transform's component by a scalar and returns the result.
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn mul_s(&self, x: T) -> Self {
        TypedTransform3D::row_major(
            self.m11 * x, self.m12 * x, self.m13 * x, self.m14 * x,
            self.m21 * x, self.m22 * x, self.m23 * x, self.m24 * x,
            self.m31 * x, self.m32 * x, self.m33 * x, self.m34 * x,
            self.m41 * x, self.m42 * x, self.m43 * x, self.m44 * x
        )
    }

    /// Convenience function to create a scale transform from a TypedScale.
    pub fn from_scale(scale: TypedScale<T, Src, Dst>) -> Self {
        TypedTransform3D::create_scale(scale.get(), scale.get(), scale.get())
    }

    /// Returns the given 2d point transformed by this transform.
    ///
    /// The input point must be use the unit Src, and the returned point has the unit Dst.
    #[inline]
    pub fn transform_point2d(&self, p: &TypedPoint2D<T, Src>) -> TypedPoint2D<T, Dst> {
        let x = p.x * self.m11 + p.y * self.m21 + self.m41;
        let y = p.x * self.m12 + p.y * self.m22 + self.m42;

        let w = p.x * self.m14 + p.y * self.m24 + self.m44;

        point2(x/w, y/w)
    }

    /// Returns the given 2d vector transformed by this matrix.
    ///
    /// The input point must be use the unit Src, and the returned point has the unit Dst.
    #[inline]
    pub fn transform_vector2d(&self, v: &TypedVector2D<T, Src>) -> TypedVector2D<T, Dst> {
        vec2(
            v.x * self.m11 + v.y * self.m21,
            v.x * self.m12 + v.y * self.m22,
        )
    }

    /// Returns the given 3d point transformed by this transform.
    ///
    /// The input point must be use the unit Src, and the returned point has the unit Dst.
    #[inline]
    pub fn transform_point3d(&self, p: &TypedPoint3D<T, Src>) -> TypedPoint3D<T, Dst> {
        let x = p.x * self.m11 + p.y * self.m21 + p.z * self.m31 + self.m41;
        let y = p.x * self.m12 + p.y * self.m22 + p.z * self.m32 + self.m42;
        let z = p.x * self.m13 + p.y * self.m23 + p.z * self.m33 + self.m43;
        let w = p.x * self.m14 + p.y * self.m24 + p.z * self.m34 + self.m44;

        point3(x/w, y/w, z/w)
    }

    /// Returns the given 3d vector transformed by this matrix.
    ///
    /// The input point must be use the unit Src, and the returned point has the unit Dst.
    #[inline]
    pub fn transform_vector3d(&self, v: &TypedVector3D<T, Src>) -> TypedVector3D<T, Dst> {
        vec3(
            v.x * self.m11 + v.y * self.m21 + v.z * self.m31,
            v.x * self.m12 + v.y * self.m22 + v.z * self.m32,
            v.x * self.m13 + v.y * self.m23 + v.z * self.m33,
        )
    }

    /// Returns a rectangle that encompasses the result of transforming the given rectangle by this
    /// transform.
    pub fn transform_rect(&self, rect: &TypedRect<T, Src>) -> TypedRect<T, Dst> {
        TypedRect::from_points(&[
            self.transform_point2d(&rect.origin),
            self.transform_point2d(&rect.top_right()),
            self.transform_point2d(&rect.bottom_left()),
            self.transform_point2d(&rect.bottom_right()),
        ])
    }

    /// Create a 3d translation transform
    pub fn create_translation(x: T, y: T, z: T) -> Self {
        let (_0, _1): (T, T) = (Zero::zero(), One::one());
        TypedTransform3D::row_major(
            _1, _0, _0, _0,
            _0, _1, _0, _0,
            _0, _0, _1, _0,
             x,  y,  z, _1
        )
    }

    /// Returns a transform with a translation applied before self's transformation.
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn pre_translate(&self, v: TypedVector3D<T, Src>) -> Self {
        self.pre_mul(&TypedTransform3D::create_translation(v.x, v.y, v.z))
    }

    /// Returns a transform with a translation applied after self's transformation.
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn post_translate(&self, v: TypedVector3D<T, Dst>) -> Self {
        self.post_mul(&TypedTransform3D::create_translation(v.x, v.y, v.z))
    }

    /// Create a 3d scale transform
    pub fn create_scale(x: T, y: T, z: T) -> Self {
        let (_0, _1): (T, T) = (Zero::zero(), One::one());
        TypedTransform3D::row_major(
             x, _0, _0, _0,
            _0,  y, _0, _0,
            _0, _0,  z, _0,
            _0, _0, _0, _1
        )
    }

    /// Returns a transform with a scale applied before self's transformation.
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn pre_scale(&self, x: T, y: T, z: T) -> Self {
        TypedTransform3D::row_major(
            self.m11 * x, self.m12,     self.m13,     self.m14,
            self.m21    , self.m22 * y, self.m23,     self.m24,
            self.m31    , self.m32,     self.m33 * z, self.m34,
            self.m41    , self.m42,     self.m43,     self.m44
        )
    }

    /// Returns a transform with a scale applied after self's transformation.
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn post_scale(&self, x: T, y: T, z: T) -> Self {
        self.post_mul(&TypedTransform3D::create_scale(x, y, z))
    }

    /// Create a 3d rotation transform from an angle / axis.
    /// The supplied axis must be normalized.
    pub fn create_rotation(x: T, y: T, z: T, theta: Angle<T>) -> Self {
        let (_0, _1): (T, T) = (Zero::zero(), One::one());
        let _2 = _1 + _1;

        let xx = x * x;
        let yy = y * y;
        let zz = z * z;

        let half_theta = theta.get() / _2;
        let sc = half_theta.sin() * half_theta.cos();
        let sq = half_theta.sin() * half_theta.sin();

        TypedTransform3D::row_major(
            _1 - _2 * (yy + zz) * sq,
            _2 * (x * y * sq - z * sc),
            _2 * (x * z * sq + y * sc),
            _0,

            _2 * (x * y * sq + z * sc),
            _1 - _2 * (xx + zz) * sq,
            _2 * (y * z * sq - x * sc),
            _0,

            _2 * (x * z * sq - y * sc),
            _2 * (y * z * sq + x * sc),
            _1 - _2 * (xx + yy) * sq,
            _0,

            _0,
            _0,
            _0,
            _1
        )
    }

    /// Returns a transform with a rotation applied after self's transformation.
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn post_rotate(&self, x: T, y: T, z: T, theta: Angle<T>) -> Self {
        self.post_mul(&TypedTransform3D::create_rotation(x, y, z, theta))
    }

    /// Returns a transform with a rotation applied before self's transformation.
    #[cfg_attr(feature = "unstable", must_use)]
    pub fn pre_rotate(&self, x: T, y: T, z: T, theta: Angle<T>) -> Self {
        self.pre_mul(&TypedTransform3D::create_rotation(x, y, z, theta))
    }

    /// Create a 2d skew transform.
    ///
    /// See https://drafts.csswg.org/css-transforms/#funcdef-skew
    pub fn create_skew(alpha: Angle<T>, beta: Angle<T>) -> Self {
        let (_0, _1): (T, T) = (Zero::zero(), One::one());
        let (sx, sy) = (beta.get().tan(), alpha.get().tan());
        TypedTransform3D::row_major(
            _1, sx, _0, _0,
            sy, _1, _0, _0,
            _0, _0, _1, _0,
            _0, _0, _0, _1
        )
    }

    /// Create a simple perspective projection transform
    pub fn create_perspective(d: T) -> Self {
        let (_0, _1): (T, T) = (Zero::zero(), One::one());
        TypedTransform3D::row_major(
            _1, _0, _0, _0,
            _0, _1, _0, _0,
            _0, _0, _1, -_1 / d,
            _0, _0, _0, _1
        )
    }
}

impl<T: Copy, Src, Dst> TypedTransform3D<T, Src, Dst> {
    /// Returns an array containing this transform's terms in row-major order (the order
    /// in which the transform is actually laid out in memory).
    pub fn to_row_major_array(&self) -> [T; 16] {
        [
            self.m11, self.m12, self.m13, self.m14,
            self.m21, self.m22, self.m23, self.m24,
            self.m31, self.m32, self.m33, self.m34,
            self.m41, self.m42, self.m43, self.m44
        ]
    }

    /// Returns an array containing this transform's terms in column-major order.
    pub fn to_column_major_array(&self) -> [T; 16] {
        [
            self.m11, self.m21, self.m31, self.m41,
            self.m12, self.m22, self.m32, self.m42,
            self.m13, self.m23, self.m33, self.m43,
            self.m14, self.m24, self.m34, self.m44
        ]
    }

    /// Returns an array containing this transform's 4 rows in (in row-major order)
    /// as arrays.
    ///
    /// This is a convenience method to interface with other libraries like glium.
    pub fn to_row_arrays(&self) -> [[T; 4]; 4] {
        [
            [self.m11, self.m12, self.m13, self.m14],
            [self.m21, self.m22, self.m23, self.m24],
            [self.m31, self.m32, self.m33, self.m34],
            [self.m41, self.m42, self.m43, self.m44]
        ]
    }

    /// Returns an array containing this transform's 4 columns in (in row-major order,
    /// or 4 rows in column-major order) as arrays.
    ///
    /// This is a convenience method to interface with other libraries like glium.
    pub fn to_column_arrays(&self) -> [[T; 4]; 4] {
        [
            [self.m11, self.m21, self.m31, self.m41],
            [self.m12, self.m22, self.m32, self.m42],
            [self.m13, self.m23, self.m33, self.m43],
            [self.m14, self.m24, self.m34, self.m44]
        ]
    }

    /// Creates a transform from an array of 16 elements in row-major order.
    pub fn from_array(array: [T; 16]) -> Self {
        Self::row_major(
            array[0],  array[1],  array[2],  array[3],
            array[4],  array[5],  array[6],  array[7],
            array[8],  array[9],  array[10], array[11],
            array[12], array[13], array[14], array[15],
        )
    }

    /// Creates a transform from 4 rows of 4 elements (row-major order).
    pub fn from_row_arrays(array: [[T; 4]; 4]) -> Self {
        Self::row_major(
            array[0][0], array[0][1], array[0][2], array[0][3],
            array[1][0], array[1][1], array[1][2], array[1][3],
            array[2][0], array[2][1], array[2][2], array[2][3],
            array[3][0], array[3][1], array[3][2], array[3][3],
        )
    }
}

impl<T0: NumCast + Copy, Src, Dst> TypedTransform3D<T0, Src, Dst> {
    /// Cast from one numeric representation to another, preserving the units.
    pub fn cast<T1: NumCast + Copy>(&self) -> Option<TypedTransform3D<T1, Src, Dst>> {
        match (NumCast::from(self.m11), NumCast::from(self.m12),
               NumCast::from(self.m13), NumCast::from(self.m14),
               NumCast::from(self.m21), NumCast::from(self.m22),
               NumCast::from(self.m23), NumCast::from(self.m24),
               NumCast::from(self.m31), NumCast::from(self.m32),
               NumCast::from(self.m33), NumCast::from(self.m34),
               NumCast::from(self.m41), NumCast::from(self.m42),
               NumCast::from(self.m43), NumCast::from(self.m44)) {
            (Some(m11), Some(m12), Some(m13), Some(m14),
             Some(m21), Some(m22), Some(m23), Some(m24),
             Some(m31), Some(m32), Some(m33), Some(m34),
             Some(m41), Some(m42), Some(m43), Some(m44)) => {
                Some(TypedTransform3D::row_major(m11, m12, m13, m14,
                                                 m21, m22, m23, m24,
                                                 m31, m32, m33, m34,
                                                 m41, m42, m43, m44))
            },
            _ => None
        }
    }
}

impl<T, Src, Dst> fmt::Debug for TypedTransform3D<T, Src, Dst>
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

#[cfg(test)]
mod tests {
    use approxeq::ApproxEq;
    use transform2d::Transform2D;
    use point::{Point2D, Point3D};
    use Angle;
    use super::*;

    use std::f32::consts::{FRAC_PI_2, PI};

    type Mf32 = Transform3D<f32>;

    // For convenience.
    fn rad(v: f32) -> Angle<f32> { Angle::radians(v) }

    #[test]
    pub fn test_translation() {
        let t1 = Mf32::create_translation(1.0, 2.0, 3.0);
        let t2 = Mf32::identity().pre_translate(vec3(1.0, 2.0, 3.0));
        let t3 = Mf32::identity().post_translate(vec3(1.0, 2.0, 3.0));
        assert_eq!(t1, t2);
        assert_eq!(t1, t3);

        assert_eq!(t1.transform_point3d(&Point3D::new(1.0, 1.0, 1.0)), Point3D::new(2.0, 3.0, 4.0));
        assert_eq!(t1.transform_point2d(&Point2D::new(1.0, 1.0)), Point2D::new(2.0, 3.0));

        assert_eq!(t1.post_mul(&t1), Mf32::create_translation(2.0, 4.0, 6.0));

        assert!(!t1.is_2d());
        assert_eq!(Mf32::create_translation(1.0, 2.0, 3.0).to_2d(), Transform2D::create_translation(1.0, 2.0));
    }

    #[test]
    pub fn test_rotation() {
        let r1 = Mf32::create_rotation(0.0, 0.0, 1.0, rad(FRAC_PI_2));
        let r2 = Mf32::identity().pre_rotate(0.0, 0.0, 1.0, rad(FRAC_PI_2));
        let r3 = Mf32::identity().post_rotate(0.0, 0.0, 1.0, rad(FRAC_PI_2));
        assert_eq!(r1, r2);
        assert_eq!(r1, r3);

        assert!(r1.transform_point3d(&Point3D::new(1.0, 2.0, 3.0)).approx_eq(&Point3D::new(2.0, -1.0, 3.0)));
        assert!(r1.transform_point2d(&Point2D::new(1.0, 2.0)).approx_eq(&Point2D::new(2.0, -1.0)));

        assert!(r1.post_mul(&r1).approx_eq(&Mf32::create_rotation(0.0, 0.0, 1.0, rad(FRAC_PI_2*2.0))));

        assert!(r1.is_2d());
        assert!(r1.to_2d().approx_eq(&Transform2D::create_rotation(rad(FRAC_PI_2))));
    }

    #[test]
    pub fn test_scale() {
        let s1 = Mf32::create_scale(2.0, 3.0, 4.0);
        let s2 = Mf32::identity().pre_scale(2.0, 3.0, 4.0);
        let s3 = Mf32::identity().post_scale(2.0, 3.0, 4.0);
        assert_eq!(s1, s2);
        assert_eq!(s1, s3);

        assert!(s1.transform_point3d(&Point3D::new(2.0, 2.0, 2.0)).approx_eq(&Point3D::new(4.0, 6.0, 8.0)));
        assert!(s1.transform_point2d(&Point2D::new(2.0, 2.0)).approx_eq(&Point2D::new(4.0, 6.0)));

        assert_eq!(s1.post_mul(&s1), Mf32::create_scale(4.0, 9.0, 16.0));

        assert!(!s1.is_2d());
        assert_eq!(Mf32::create_scale(2.0, 3.0, 0.0).to_2d(), Transform2D::create_scale(2.0, 3.0));
    }

    #[test]
    pub fn test_ortho() {
        let (left, right, bottom, top) = (0.0f32, 1.0f32, 0.1f32, 1.0f32);
        let (near, far) = (-1.0f32, 1.0f32);
        let result = Mf32::ortho(left, right, bottom, top, near, far);
        let expected = Mf32::row_major(
             2.0,  0.0,         0.0, 0.0,
             0.0,  2.22222222,  0.0, 0.0,
             0.0,  0.0,        -1.0, 0.0,
            -1.0, -1.22222222, -0.0, 1.0
        );
        debug!("result={:?} expected={:?}", result, expected);
        assert!(result.approx_eq(&expected));
    }

    #[test]
    pub fn test_is_2d() {
        assert!(Mf32::identity().is_2d());
        assert!(Mf32::create_rotation(0.0, 0.0, 1.0, rad(0.7854)).is_2d());
        assert!(!Mf32::create_rotation(0.0, 1.0, 0.0, rad(0.7854)).is_2d());
    }

    #[test]
    pub fn test_row_major_2d() {
        let m1 = Mf32::row_major_2d(1.0, 2.0, 3.0, 4.0, 5.0, 6.0);
        let m2 = Mf32::row_major(
            1.0, 2.0, 0.0, 0.0,
            3.0, 4.0, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            5.0, 6.0, 0.0, 1.0
        );
        assert_eq!(m1, m2);
    }

    #[test]
    fn test_column_major() {
        assert_eq!(
            Mf32::row_major(
                1.0,  2.0,  3.0,  4.0,
                5.0,  6.0,  7.0,  8.0,
                9.0,  10.0, 11.0, 12.0,
                13.0, 14.0, 15.0, 16.0,
            ),
            Mf32::column_major(
                1.0,  5.0,  9.0,  13.0,
                2.0,  6.0,  10.0, 14.0,
                3.0,  7.0,  11.0, 15.0,
                4.0,  8.0,  12.0, 16.0,
            )
        );
    }

    #[test]
    pub fn test_inverse_simple() {
        let m1 = Mf32::identity();
        let m2 = m1.inverse().unwrap();
        assert!(m1.approx_eq(&m2));
    }

    #[test]
    pub fn test_inverse_scale() {
        let m1 = Mf32::create_scale(1.5, 0.3, 2.1);
        let m2 = m1.inverse().unwrap();
        assert!(m1.pre_mul(&m2).approx_eq(&Mf32::identity()));
    }

    #[test]
    pub fn test_inverse_translate() {
        let m1 = Mf32::create_translation(-132.0, 0.3, 493.0);
        let m2 = m1.inverse().unwrap();
        assert!(m1.pre_mul(&m2).approx_eq(&Mf32::identity()));
    }

    #[test]
    pub fn test_inverse_rotate() {
        let m1 = Mf32::create_rotation(0.0, 1.0, 0.0, rad(1.57));
        let m2 = m1.inverse().unwrap();
        assert!(m1.pre_mul(&m2).approx_eq(&Mf32::identity()));
    }

    #[test]
    pub fn test_inverse_transform_point_2d() {
        let m1 = Mf32::create_translation(100.0, 200.0, 0.0);
        let m2 = m1.inverse().unwrap();
        assert!(m1.pre_mul(&m2).approx_eq(&Mf32::identity()));

        let p1 = Point2D::new(1000.0, 2000.0);
        let p2 = m1.transform_point2d(&p1);
        assert!(p2.eq(&Point2D::new(1100.0, 2200.0)));

        let p3 = m2.transform_point2d(&p2);
        assert!(p3.eq(&p1));
    }

    #[test]
    fn test_inverse_none() {
        assert!(Mf32::create_scale(2.0, 0.0, 2.0).inverse().is_none());
        assert!(Mf32::create_scale(2.0, 2.0, 2.0).inverse().is_some());
    }

    #[test]
    pub fn test_pre_post() {
        let m1 = Transform3D::identity().post_scale(1.0, 2.0, 3.0).post_translate(vec3(1.0, 2.0, 3.0));
        let m2 = Transform3D::identity().pre_translate(vec3(1.0, 2.0, 3.0)).pre_scale(1.0, 2.0, 3.0);
        assert!(m1.approx_eq(&m2));

        let r = Mf32::create_rotation(0.0, 0.0, 1.0, rad(FRAC_PI_2));
        let t = Mf32::create_translation(2.0, 3.0, 0.0);

        let a = Point3D::new(1.0, 1.0, 1.0);

        assert!(r.post_mul(&t).transform_point3d(&a).approx_eq(&Point3D::new(3.0, 2.0, 1.0)));
        assert!(t.post_mul(&r).transform_point3d(&a).approx_eq(&Point3D::new(4.0, -3.0, 1.0)));
        assert!(t.post_mul(&r).transform_point3d(&a).approx_eq(&r.transform_point3d(&t.transform_point3d(&a))));

        assert!(r.pre_mul(&t).transform_point3d(&a).approx_eq(&Point3D::new(4.0, -3.0, 1.0)));
        assert!(t.pre_mul(&r).transform_point3d(&a).approx_eq(&Point3D::new(3.0, 2.0, 1.0)));
        assert!(t.pre_mul(&r).transform_point3d(&a).approx_eq(&t.transform_point3d(&r.transform_point3d(&a))));
    }

    #[test]
    fn test_size_of() {
        use std::mem::size_of;
        assert_eq!(size_of::<Transform3D<f32>>(), 16*size_of::<f32>());
        assert_eq!(size_of::<Transform3D<f64>>(), 16*size_of::<f64>());
    }

    #[test]
    pub fn test_transform_associativity() {
        let m1 = Mf32::row_major(3.0, 2.0, 1.5, 1.0,
                                 0.0, 4.5, -1.0, -4.0,
                                 0.0, 3.5, 2.5, 40.0,
                                 0.0, 3.0, 0.0, 1.0);
        let m2 = Mf32::row_major(1.0, -1.0, 3.0, 0.0,
                                 -1.0, 0.5, 0.0, 2.0,
                                 1.5, -2.0, 6.0, 0.0,
                                 -2.5, 6.0, 1.0, 1.0);

        let p = Point3D::new(1.0, 3.0, 5.0);
        let p1 = m2.pre_mul(&m1).transform_point3d(&p);
        let p2 = m2.transform_point3d(&m1.transform_point3d(&p));
        assert!(p1.approx_eq(&p2));
    }

    #[test]
    pub fn test_is_identity() {
        let m1 = Transform3D::identity();
        assert!(m1.is_identity());
        let m2 = m1.post_translate(vec3(0.1, 0.0, 0.0));
        assert!(!m2.is_identity());
    }

    #[test]
    pub fn test_transform_vector() {
        // Translation does not apply to vectors.
        let m = Mf32::create_translation(1.0, 2.0, 3.0);
        let v1 = vec3(10.0, -10.0, 3.0);
        assert_eq!(v1, m.transform_vector3d(&v1));
        // While it does apply to points.
        assert!(v1.to_point() != m.transform_point3d(&v1.to_point()));

        // same thing with 2d vectors/points
        let v2 = vec2(10.0, -5.0);
        assert_eq!(v2, m.transform_vector2d(&v2));
        assert!(v2.to_point() != m.transform_point2d(&v2.to_point()));
    }

    #[test]
    pub fn test_is_backface_visible() {
        // backface is not visible for rotate-x 0 degree.
        let r1 = Mf32::create_rotation(1.0, 0.0, 0.0, rad(0.0));
        assert!(!r1.is_backface_visible());
        // backface is not visible for rotate-x 45 degree.
        let r1 = Mf32::create_rotation(1.0, 0.0, 0.0, rad(PI * 0.25));
        assert!(!r1.is_backface_visible());
        // backface is visible for rotate-x 180 degree.
        let r1 = Mf32::create_rotation(1.0, 0.0, 0.0, rad(PI));
        assert!(r1.is_backface_visible());
        // backface is visible for rotate-x 225 degree.
        let r1 = Mf32::create_rotation(1.0, 0.0, 0.0, rad(PI * 1.25));
        assert!(r1.is_backface_visible());
        // backface is not visible for non-inverseable matrix
        let r1 = Mf32::create_scale(2.0, 0.0, 2.0);
        assert!(!r1.is_backface_visible());
    }
}
