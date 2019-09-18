// Copyright 2013-2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A Big integer (signed version: `BigInt`, unsigned version: `BigUint`).
//!
//! A `BigUint` is represented as a vector of `BigDigit`s.
//! A `BigInt` is a combination of `BigUint` and `Sign`.
//!
//! Common numerical operations are overloaded, so we can treat them
//! the same way we treat other numbers.
//!
//! ## Example
//!
//! ```rust
//! extern crate num_bigint;
//! extern crate num_traits;
//!
//! # fn main() {
//! use num_bigint::BigUint;
//! use num_traits::{Zero, One};
//! use std::mem::replace;
//!
//! // Calculate large fibonacci numbers.
//! fn fib(n: usize) -> BigUint {
//!     let mut f0: BigUint = Zero::zero();
//!     let mut f1: BigUint = One::one();
//!     for _ in 0..n {
//!         let f2 = f0 + &f1;
//!         // This is a low cost way of swapping f0 with f1 and f1 with f2.
//!         f0 = replace(&mut f1, f2);
//!     }
//!     f0
//! }
//!
//! // This is a very large number.
//! println!("fib(1000) = {}", fib(1000));
//! # }
//! ```
//!
//! It's easy to generate large random numbers:
//!
//! ```rust
//! extern crate rand;
//! extern crate num_bigint as bigint;
//!
//! # #[cfg(feature = "rand")]
//! # fn main() {
//! use bigint::{ToBigInt, RandBigInt};
//!
//! let mut rng = rand::thread_rng();
//! let a = rng.gen_bigint(1000);
//!
//! let low = -10000.to_bigint().unwrap();
//! let high = 10000.to_bigint().unwrap();
//! let b = rng.gen_bigint_range(&low, &high);
//!
//! // Probably an even larger number.
//! println!("{}", a * b);
//! # }
//!
//! # #[cfg(not(feature = "rand"))]
//! # fn main() {
//! # }
//! ```
//!
//! ## Compatibility
//!
//! The `num-bigint` crate is tested for rustc 1.8 and greater.

#![doc(html_root_url = "https://docs.rs/num-bigint/0.1")]

#[cfg(any(feature = "rand", test))]
extern crate rand;
#[cfg(feature = "rustc-serialize")]
extern crate rustc_serialize;
#[cfg(feature = "serde")]
extern crate serde;

extern crate num_integer as integer;
extern crate num_traits as traits;

use std::error::Error;
use std::num::ParseIntError;
use std::fmt;

#[cfg(target_pointer_width = "32")]
type UsizePromotion = u32;
#[cfg(target_pointer_width = "64")]
type UsizePromotion = u64;

#[cfg(target_pointer_width = "32")]
type IsizePromotion = i32;
#[cfg(target_pointer_width = "64")]
type IsizePromotion = i64;

#[derive(Debug, PartialEq)]
pub enum ParseBigIntError {
    ParseInt(ParseIntError),
    Other,
}

impl fmt::Display for ParseBigIntError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self {
            &ParseBigIntError::ParseInt(ref e) => e.fmt(f),
            &ParseBigIntError::Other => "failed to parse provided string".fmt(f),
        }
    }
}

impl Error for ParseBigIntError {
    fn description(&self) -> &str {
        "failed to parse bigint/biguint"
    }
}

impl From<ParseIntError> for ParseBigIntError {
    fn from(err: ParseIntError) -> ParseBigIntError {
        ParseBigIntError::ParseInt(err)
    }
}

#[cfg(test)]
use std::hash;

#[cfg(test)]
fn hash<T: hash::Hash>(x: &T) -> u64 {
    use std::hash::{BuildHasher, Hasher};
    use std::collections::hash_map::RandomState;
    let mut hasher = <RandomState as BuildHasher>::Hasher::new();
    x.hash(&mut hasher);
    hasher.finish()
}

#[macro_use]
mod macros;

mod biguint;
mod bigint;

pub use biguint::BigUint;
pub use biguint::ToBigUint;
pub use biguint::big_digit;
pub use biguint::big_digit::{BigDigit, DoubleBigDigit, ZERO_BIG_DIGIT};

pub use bigint::Sign;
pub use bigint::BigInt;
pub use bigint::ToBigInt;
pub use bigint::RandBigInt;
