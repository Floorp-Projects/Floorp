//!
//! A Decimal implementation written in pure Rust suitable
//! for financial calculations that require significant integral
//! and fractional digits with no round-off errors.
//!
//! The binary representation consists of a 96 bit integer number,
//! a scaling factor used to specify the decimal fraction and a 1
//! bit sign. Because of this representation, trailing zeros are
//! preserved and may be exposed when in string form. These can be
//! truncated using the `normalize` or `round_dp` functions.
//!
//! ## Usage
//!
//! Decimal numbers can be created in a few distinct ways, depending
//! on the rust compiler version you're targeting.
//!
//! The stable version of rust requires you to create a Decimal number
//! using one of it's convenience methods.
//!
//! ```rust
//! use rust_decimal::prelude::*;
//!
//! // Using an integer followed by the decimal points
//! let scaled = Decimal::new(202, 2); // 2.02
//!
//! // From a string representation
//! let from_string = Decimal::from_str("2.02").unwrap(); // 2.02
//!
//! // Using the `Into` trait
//! let my_int : Decimal = 3i32.into();
//!
//! // Using the raw decimal representation
//! // 3.1415926535897932384626433832
//! let pi = Decimal::from_parts(1102470952, 185874565, 1703060790, false, 28);
//! ```
//!
mod decimal;
mod error;

#[cfg(any(feature = "postgres", feature = "diesel"))]
mod postgres;
#[cfg(feature = "serde")]
mod serde_types;

pub use decimal::{Decimal, RoundingStrategy};
pub use error::Error;

pub mod prelude {
    pub use crate::{Decimal, RoundingStrategy};
    pub use num_traits::{FromPrimitive, One, ToPrimitive, Zero};
    pub use std::str::FromStr;
}

#[cfg(feature = "diesel")]
#[macro_use]
extern crate diesel;
