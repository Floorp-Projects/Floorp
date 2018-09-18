// Copyright 2014-2016 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A collection of numeric types and traits for Rust.
//!
//! This includes new types for big integers, rationals, and complex numbers,
//! new traits for generic programming on numeric properties like `Integer`,
//! and generic range iterators.
//!
//! ## Example
//!
//! This example uses the BigRational type and [Newton's method][newt] to
//! approximate a square root to arbitrary precision:
//!
//! ```
//! extern crate num;
//! # #[cfg(all(feature = "bigint", feature="rational"))]
//! # mod test {
//!
//! use num::FromPrimitive;
//! use num::bigint::BigInt;
//! use num::rational::{Ratio, BigRational};
//!
//! # pub
//! fn approx_sqrt(number: u64, iterations: usize) -> BigRational {
//!     let start: Ratio<BigInt> = Ratio::from_integer(FromPrimitive::from_u64(number).unwrap());
//!     let mut approx = start.clone();
//!
//!     for _ in 0..iterations {
//!         approx = (&approx + (&start / &approx)) /
//!             Ratio::from_integer(FromPrimitive::from_u64(2).unwrap());
//!     }
//!
//!     approx
//! }
//! # }
//! # #[cfg(not(all(feature = "bigint", feature="rational")))]
//! # mod test { pub fn approx_sqrt(n: u64, _: usize) -> u64 { n } }
//! # use test::approx_sqrt;
//!
//! fn main() {
//!     println!("{}", approx_sqrt(10, 4)); // prints 4057691201/1283082416
//! }
//!
//! ```
//!
//! [newt]: https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Babylonian_method
#![doc(html_logo_url = "https://rust-num.github.io/num/rust-logo-128x128-blk-v2.png",
       html_favicon_url = "https://rust-num.github.io/num/favicon.ico",
       html_root_url = "https://rust-num.github.io/num/",
       html_playground_url = "http://play.integer32.com/")]

extern crate num_traits;
extern crate num_integer;
extern crate num_iter;
#[cfg(feature = "num-complex")]
extern crate num_complex;
#[cfg(feature = "num-bigint")]
extern crate num_bigint;
#[cfg(feature = "num-rational")]
extern crate num_rational;

#[cfg(feature = "num-bigint")]
pub use num_bigint::{BigInt, BigUint};
#[cfg(feature = "num-rational")]
pub use num_rational::Rational;
#[cfg(all(feature = "num-rational", feature="num-bigint"))]
pub use num_rational::BigRational;
#[cfg(feature = "num-complex")]
pub use num_complex::Complex;
pub use num_integer::Integer;
pub use num_iter::{range, range_inclusive, range_step, range_step_inclusive};
pub use num_traits::{Num, Zero, One, Signed, Unsigned, Bounded,
                     one, zero, abs, abs_sub, signum,
                     Saturating, CheckedAdd, CheckedSub, CheckedMul, CheckedDiv,
                     PrimInt, Float, ToPrimitive, FromPrimitive, NumCast, cast,
                     pow, checked_pow, clamp};

#[cfg(feature = "num-bigint")]
pub mod bigint {
    pub use num_bigint::*;
}

#[cfg(feature = "num-complex")]
pub mod complex {
    pub use num_complex::*;
}

pub mod integer {
    pub use num_integer::*;
}

pub mod iter {
    pub use num_iter::*;
}

pub mod traits {
    pub use num_traits::*;
}

#[cfg(feature = "num-rational")]
pub mod rational {
    pub use num_rational::*;
}
