// Copyright 2014-2018 Optimal Computing (NZ) Ltd.
// Licensed under the MIT license.  See LICENSE for details.

//! # float-cmp
//!
//! float-cmp defines and implements traits for approximate comparison of floating point types
//! which have fallen away from exact equality due to the limited precision available within
//! floating point representations. Implementations of these traits are provided for `f32`
//! and `f64` types.
//!
//! When I was a kid in the '80s, the programming rule was "Never compare floating point
//! numbers". If you can follow that rule and still get the outcome you desire, then more
//! power to you. However, if you really do need to compare them, this crate provides a
//! reasonable way to do so.
//!
//! Another crate `efloat` offers another solution by providing a floating point type that
//! tracks its error bounds as operations are performed on it, and thus can implement the
//! `ApproxEq` trait in this crate more accurately, without specifying a `Margin`.
//!
//! The recommended go-to solution (although it may not be appropriate in all cases) is the
//! `approx_eq()` function in the `ApproxEq` trait (or better yet, the macros).  For `f32`
//! and `f64`, the `F32Margin` and `F64Margin` types are provided for specifying margins as
//! both an epsilon value and an ULPs value, and defaults are provided via `Default`
//! (although there is no perfect default value that is always appropriate, so beware).
//!
//! Several other traits are provided including `Ulps`, `ApproxEqUlps`, `ApproxOrdUlps`, and
//! `ApproxEqRatio`.
//!
//! ## The problem
//!
//! Floating point operations must round answers to the nearest representable number. Multiple
//! operations may result in an answer different from what you expect. In the following example,
//! the assert will fail, even though the printed output says "0.45 == 0.45":
//!
//! ```should_panic
//!   # extern crate float_cmp;
//!   # use float_cmp::ApproxEq;
//!   # fn main() {
//!   let a: f32 = 0.15 + 0.15 + 0.15;
//!   let b: f32 = 0.1 + 0.1 + 0.25;
//!   println!("{} == {}", a, b);
//!   assert!(a==b)  // Fails, because they are not exactly equal
//!   # }
//! ```
//!
//! This fails because the correct answer to most operations isn't exactly representable, and so
//! your computer's processor chooses to represent the answer with the closest value it has
//! available. This introduces error, and this error can accumulate as multiple operations are
//! performed.
//!
//! ## The solution
//!
//! With `ApproxEq`, we can get the answer we intend:
//!
//! ```
//!   # #[macro_use]
//!   # extern crate float_cmp;
//!   # use float_cmp::{ApproxEq, F32Margin};
//!   # fn main() {
//!   let a: f32 = 0.15 + 0.15 + 0.15;
//!   let b: f32 = 0.1 + 0.1 + 0.25;
//!   println!("{} == {}", a, b);
//!   // They are equal, within 2 ulps
//!   assert!( approx_eq!(f32, a, b, ulps = 2) );
//!   # }
//! ```
//!
//! ## Some explanation
//!
//! We use the term ULP (units of least precision, or units in the last place) to mean the
//! difference between two adjacent floating point representations (adjacent meaning that there is
//! no floating point number between them). This term is borrowed from prior work (personally I
//! would have chosen "quanta"). The size of an ULP (measured as a float) varies
//! depending on the exponents of the floating point numbers in question. That is a good thing,
//! because as numbers fall away from equality due to the imprecise nature of their representation,
//! they fall away in ULPs terms, not in absolute terms.  Pure epsilon-based comparisons are
//! absolute and thus don't map well to the nature of the additive error issue. They work fine
//! for many ranges of numbers, but not for others (consider comparing -0.0000000028
//! to +0.00000097).
//!
//! ## Using this crate
//!
//! You can use the `ApproxEq` trait directly like so:
//!
//! ```
//!   # extern crate float_cmp;
//!   # use float_cmp::{ApproxEq, F32Margin};
//!   # fn main() {
//!   # let a: f32 = 0.15 + 0.15 + 0.15;
//!   # let b: f32 = 0.1 + 0.1 + 0.25;
//!     assert!( a.approx_eq(b, F32Margin { ulps: 2, epsilon: 0.0 }) );
//!   # }
//! ```
//!
//! We have implemented `From<(f32,i32)>` for `F32Margin` (and similarly for `F64Margin`)
//! so you can use this shorthand:
//!
//! ```
//!   # extern crate float_cmp;
//!   # use float_cmp::{ApproxEq, F32Margin};
//!   # fn main() {
//!   # let a: f32 = 0.15 + 0.15 + 0.15;
//!   # let b: f32 = 0.1 + 0.1 + 0.25;
//!     assert!( a.approx_eq(b, (0.0, 2)) );
//!   # }
//! ```
//!
//! With macros, it is easier to be explicit about which type of margin you wish to set,
//! without mentioning the other one (the other one will be zero). But the downside is
//! that you have to specify the type you are dealing with:
//!
//! ```
//!   # #[macro_use]
//!   # extern crate float_cmp;
//!   # use float_cmp::{ApproxEq, F32Margin};
//!   # fn main() {
//!   # let a: f32 = 0.15 + 0.15 + 0.15;
//!   # let b: f32 = 0.1 + 0.1 + 0.25;
//!     assert!( approx_eq!(f32, a, b, ulps = 2) );
//!     assert!( approx_eq!(f32, a, b, epsilon = 0.00000003) );
//!     assert!( approx_eq!(f32, a, b, epsilon = 0.00000003, ulps = 2) );
//!     assert!( approx_eq!(f32, a, b, (0.0, 2)) );
//!     assert!( approx_eq!(f32, a, b, F32Margin { epsilon: 0.0, ulps: 2 }) );
//!     assert!( approx_eq!(f32, a, b, F32Margin::default()) );
//!     assert!( approx_eq!(f32, a, b) ); // uses the default
//!   # }
//! ```
//!
//! For most cases, I recommend you use a smallish integer for the `ulps` parameter (1 to 5
//! or so), and a similar small multiple of the floating point's EPSILON constant (1.0 to 5.0
//! or so), but there are *plenty* of cases where this is insufficient.
//!
//! ## Implementing these traits
//!
//! You can implement `ApproxEq` for your own complex types like shown below.
//! The floating point type `F` must be `Copy`, but for large types you can implement
//! it for references to your type as shown.
//!
//! ```
//! use float_cmp::ApproxEq;
//!
//! pub struct Vec2<F> {
//!   pub x: F,
//!   pub y: F,
//! }
//!
//! impl<'a, M: Copy + Default, F: Copy + ApproxEq<Margin=M>> ApproxEq for &'a Vec2<F> {
//!   type Margin = M;
//!
//!   fn approx_eq<T: Into<Self::Margin>>(self, other: Self, margin: T) -> bool {
//!     let margin = margin.into();
//!     self.x.approx_eq(other.x, margin)
//!       && self.y.approx_eq(other.y, margin)
//!   }
//! }
//! ```
//!
//! ## Non floating-point types
//!
//! `ApproxEq` can be implemented for non floating-point types as well, since `Margin` is
//! an associated type.
//!
//! The `efloat` crate implements (or soon will implement) `ApproxEq` for a compound type
//! that tracks floating point error bounds by checking if the error bounds overlap.
//! In that case `type Margin = ()`.
//!
//! ## Inspiration
//!
//! This crate was inspired by this Random ASCII blog post:
//!
//! [https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/](https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/)

#[cfg(feature="num-traits")]
extern crate num_traits;

#[macro_use]
mod macros;

pub fn trials() {
    println!("are they approximately equal?: {:?}",
             approx_eq!(f32, 1.0, 1.0000001));
}

mod ulps;
pub use self::ulps::Ulps;

mod ulps_eq;
pub use self::ulps_eq::ApproxEqUlps;

mod eq;
pub use self::eq::{ApproxEq, F32Margin, F64Margin};

#[cfg(feature="num-traits")]
mod ratio;
#[cfg(feature="num-traits")]
pub use self::ratio::ApproxEqRatio;
