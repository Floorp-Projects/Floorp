// Copyright 2018 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use point::{TypedPoint2D, TypedPoint3D};
use vector::{TypedVector2D, TypedVector3D};

use num::{One, Zero};

use core::fmt;
use core::marker::PhantomData;
use core::ops::Div;


define_matrix! {
    /// Homogeneous vector in 3D space.
    pub struct HomogeneousVector<T, U> {
        pub x: T,
        pub y: T,
        pub z: T,
        pub w: T,
    }
}


impl<T, U> HomogeneousVector<T, U> {
    /// Constructor taking scalar values directly.
    #[inline]
    pub fn new(x: T, y: T, z: T, w: T) -> Self {
        HomogeneousVector { x, y, z, w, _unit: PhantomData }
    }
}


impl<T: Copy + Div<T, Output=T> + Zero + PartialOrd, U> HomogeneousVector<T, U> {
    /// Convert into Cartesian 2D point.
    ///
    /// Returns None if the point is on or behind the W=0 hemisphere.
    #[inline]
    pub fn to_point2d(&self) -> Option<TypedPoint2D<T, U>> {
        if self.w > T::zero() {
            Some(TypedPoint2D::new(self.x / self.w, self.y / self.w))
        } else {
            None
        }
    }

    /// Convert into Cartesian 3D point.
    ///
    /// Returns None if the point is on or behind the W=0 hemisphere.
    #[inline]
    pub fn to_point3d(&self) -> Option<TypedPoint3D<T, U>> {
        if self.w > T::zero() {
            Some(TypedPoint3D::new(self.x / self.w, self.y / self.w, self.z / self.w))
        } else {
            None
        }
    }
}

impl<T: Zero, U> From<TypedVector2D<T, U>> for HomogeneousVector<T, U> {
    #[inline]
    fn from(v: TypedVector2D<T, U>) -> Self {
        HomogeneousVector::new(v.x, v.y, T::zero(), T::zero())
    }
}

impl<T: Zero, U> From<TypedVector3D<T, U>> for HomogeneousVector<T, U> {
    #[inline]
    fn from(v: TypedVector3D<T, U>) -> Self {
        HomogeneousVector::new(v.x, v.y, v.z, T::zero())
    }
}

impl<T: Zero + One, U> From<TypedPoint2D<T, U>> for HomogeneousVector<T, U> {
    #[inline]
    fn from(p: TypedPoint2D<T, U>) -> Self {
        HomogeneousVector::new(p.x, p.y, T::zero(), T::one())
    }
}

impl<T: One, U> From<TypedPoint3D<T, U>> for HomogeneousVector<T, U> {
    #[inline]
    fn from(p: TypedPoint3D<T, U>) -> Self {
        HomogeneousVector::new(p.x, p.y, p.z, T::one())
    }
}

impl<T: fmt::Debug, U> fmt::Debug for HomogeneousVector<T, U> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "({:?},{:?},{:?},{:?})", self.x, self.y, self.z, self.w)
    }
}

impl<T: fmt::Display, U> fmt::Display for HomogeneousVector<T, U> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        write!(formatter, "({},{},{},{})", self.x, self.y, self.z, self.w)
    }
}


#[cfg(test)]
mod homogeneous {
    use super::HomogeneousVector;
    use point::{Point2D, Point3D};

    #[test]
    fn roundtrip() {
        assert_eq!(Some(Point2D::new(1.0, 2.0)), HomogeneousVector::from(Point2D::new(1.0, 2.0)).to_point2d());
        assert_eq!(Some(Point3D::new(1.0, -2.0, 0.1)), HomogeneousVector::from(Point3D::new(1.0, -2.0, 0.1)).to_point3d());
    }

    #[test]
    fn negative() {
        assert_eq!(None, HomogeneousVector::<f32, ()>::new(1.0, 2.0, 3.0, 0.0).to_point2d());
        assert_eq!(None, HomogeneousVector::<f32, ()>::new(1.0, -2.0, -3.0, -2.0).to_point3d());
    }
}
