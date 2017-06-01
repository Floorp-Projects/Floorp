// Copyright 2013 The Servo Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "unstable", feature(asm, repr_simd, test))]

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
//! All euclid types are marked `#[repr(C)]` in order to facilitate exposing them to
//! foreign function interfaces (provided the underlying scalar type is also `repr(C)`).
//!
//! ```rust
//! use euclid::*;
//! pub struct ScreenSpace;
//! pub type ScreenPoint = TypedPoint2D<f32, ScreenSpace>;
//! pub type ScreenSize = TypedSize2D<f32, ScreenSpace>;
//! pub struct WorldSpace;
//! pub type WorldPoint = TypedPoint3D<f32, WorldSpace>;
//! pub type ProjectionMatrix = TypedMatrix4D<f32, WorldSpace, ScreenSpace>;
//! // etc...
//! ```
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
extern crate rustc_serialize;
extern crate serde;

#[cfg(test)]
extern crate rand;
#[cfg(feature = "unstable")]
extern crate test;
extern crate num_traits;

pub use length::Length;
pub use scale_factor::ScaleFactor;
pub use matrix2d::{Matrix2D, TypedMatrix2D};
pub use matrix4d::{Matrix4D, TypedMatrix4D};
pub use point::{
    Point2D, TypedPoint2D,
    Point3D, TypedPoint3D,
    Point4D, TypedPoint4D,
};
pub use rect::{Rect, TypedRect};
pub use side_offsets::{SideOffsets2D, TypedSideOffsets2D};
#[cfg(feature = "unstable")] pub use side_offsets::SideOffsets2DSimdI32;
pub use size::{Size2D, TypedSize2D};

pub mod approxeq;
pub mod length;
#[macro_use]
mod macros;
pub mod matrix2d;
pub mod matrix4d;
pub mod num;
pub mod point;
pub mod rect;
pub mod scale_factor;
pub mod side_offsets;
pub mod size;
pub mod trig;

/// The default unit.
#[derive(Clone, Copy, RustcDecodable, RustcEncodable)]
pub struct UnknownUnit;

/// Unit for angles in radians.
pub struct Rad;

/// Unit for angles in degrees.
pub struct Deg;

/// A value in radians.
pub type Radians<T> = Length<T, Rad>;

/// A value in Degrees.
pub type Degrees<T> = Length<T, Deg>;
