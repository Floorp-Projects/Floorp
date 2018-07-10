// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "unstable", feature(fn_must_use))]
#![cfg_attr(not(test), no_std)]

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

#[cfg(feature = "serde")]
#[macro_use]
extern crate serde;

extern crate num_traits;
#[cfg(test)]
extern crate rand;

#[cfg(test)]
use std as core;

pub use length::Length;
pub use scale::TypedScale;
pub use transform2d::{Transform2D, TypedTransform2D};
pub use transform3d::{Transform3D, TypedTransform3D};
pub use point::{Point2D, Point3D, TypedPoint2D, TypedPoint3D, point2, point3};
pub use vector::{TypedVector2D, TypedVector3D, Vector2D, Vector3D, vec2, vec3};
pub use vector::{BoolVector2D, BoolVector3D, bvec2, bvec3};
pub use homogen::HomogeneousVector;

pub use rect::{rect, Rect, TypedRect};
pub use rotation::{Angle, Rotation2D, Rotation3D, TypedRotation2D, TypedRotation3D};
pub use side_offsets::{SideOffsets2D, TypedSideOffsets2D};
pub use size::{Size2D, TypedSize2D, size2};
pub use trig::Trig;

#[macro_use]
mod macros;

pub mod approxeq;
mod homogen;
pub mod num;
mod length;
mod point;
mod rect;
mod rotation;
mod scale;
mod side_offsets;
mod size;
mod transform2d;
mod transform3d;
mod trig;
mod vector;

/// The default unit.
#[derive(Clone, Copy)]
pub struct UnknownUnit;

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

/// Temporary alias to facilitate the transition to the new naming scheme
#[deprecated]
pub type ScaleFactor<T, Src, Dst> = TypedScale<T, Src, Dst>;

/// Temporary alias to facilitate the transition to the new naming scheme
#[deprecated]
pub use Angle as Radians;
