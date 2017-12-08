// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "unstable", feature(asm, repr_simd, test, fn_must_use))]

//! A collection of strongly typed math tools for computer graphics with an inclination
//! towards 2d graphics and layout.
//!
//! All types are generic over the scalar type of their component (`f32`, `i32`, etc.),
//! and tagged with a generic Unit parameter which is useful to prevent mixing
//! values from different spaces. For example it should not be legal to translate
//! a screen-space position by a world-space vector and this can be expressed using
//! the generic Unit parameter.
//!
//! This unit system is not mandatory and all Typed* structures have an alias
//! with the default unit: `UnknownUnit`.
//! for example ```Point2D<T>``` is equivalent to ```TypedPoint2D<T, UnknownUnit>```.
//! Client code typically creates a set of aliases for each type and doesn't need
//! to deal with the specifics of typed units further. For example:
//!
//! ```rust
//! use euclid::*;
//! pub struct ScreenSpace;
//! pub type ScreenPoint = TypedPoint2D<f32, ScreenSpace>;
//! pub type ScreenSize = TypedSize2D<f32, ScreenSpace>;
//! pub struct WorldSpace;
//! pub type WorldPoint = TypedPoint3D<f32, WorldSpace>;
//! pub type ProjectionMatrix = TypedTransform3D<f32, WorldSpace, ScreenSpace>;
//! // etc...
//! ```
//!
//! All euclid types are marked `#[repr(C)]` in order to facilitate exposing them to
//! foreign function interfaces (provided the underlying scalar type is also `repr(C)`).
//!
//! Components are accessed in their scalar form by default for convenience, and most
//! types additionally implement strongly typed accessors which return typed ```Length``` wrappers.
//! For example:
//!
//! ```rust
//! # use euclid::*;
//! # pub struct WorldSpace;
//! # pub type WorldPoint = TypedPoint3D<f32, WorldSpace>;
//! let p = WorldPoint::new(0.0, 1.0, 1.0);
//! // p.x is an f32.
//! println!("p.x = {:?} ", p.x);
//! // p.x is a Length<f32, WorldSpace>.
//! println!("p.x_typed() = {:?} ", p.x_typed());
//! // Length::get returns the scalar value (f32).
//! assert_eq!(p.x, p.x_typed().get());
//! ```

extern crate heapsize;

#[cfg_attr(test, macro_use)]
extern crate log;
extern crate serde;

#[cfg(test)]
extern crate rand;
#[cfg(feature = "unstable")]
extern crate test;
extern crate num_traits;

pub use length::Length;
pub use scale_factor::ScaleFactor;
pub use transform2d::{Transform2D, TypedTransform2D};
pub use transform3d::{Transform3D, TypedTransform3D};
pub use point::{
    Point2D, TypedPoint2D, point2,
    Point3D, TypedPoint3D, point3,
};
pub use vector::{
    Vector2D, TypedVector2D, vec2,
    Vector3D, TypedVector3D, vec3,
};

pub use rect::{Rect, TypedRect, rect};
pub use rotation::{TypedRotation2D, Rotation2D, TypedRotation3D, Rotation3D};
pub use side_offsets::{SideOffsets2D, TypedSideOffsets2D};
#[cfg(feature = "unstable")] pub use side_offsets::SideOffsets2DSimdI32;
pub use size::{Size2D, TypedSize2D, size2};
pub use trig::Trig;

pub mod approxeq;
pub mod num;
mod length;
#[macro_use]
mod macros;
mod transform2d;
mod transform3d;
mod point;
mod rect;
mod rotation;
mod scale_factor;
mod side_offsets;
mod size;
mod trig;
mod vector;

/// The default unit.
#[derive(Clone, Copy)]
pub struct UnknownUnit;

/// Unit for angles in radians.
pub struct Rad;

/// Unit for angles in degrees.
pub struct Deg;

/// A value in radians.
pub type Radians<T> = Length<T, Rad>;

/// A value in Degrees.
pub type Degrees<T> = Length<T, Deg>;

/// Temporary alias to facilitate the transition to the new naming scheme
#[deprecated]
pub type Matrix2D<T> = Transform2D<T>;

/// Temporary alias to facilitate the transition to the new naming scheme
#[deprecated]
pub type TypedMatrix2D<T, Src, Dst> = TypedTransform2D<T, Src, Dst>;

/// Temporary alias to facilitate the transition to the new naming scheme
#[deprecated]
pub type Matrix4D<T> = Transform3D<T>;

/// Temporary alias to facilitate the transition to the new naming scheme
#[deprecated]
pub type TypedMatrix4D<T, Src, Dst> = TypedTransform3D<T, Src, Dst>;
