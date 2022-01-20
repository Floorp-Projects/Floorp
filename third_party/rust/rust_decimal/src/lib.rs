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
//! ## Getting started
//!
//! To get started, add `rust_decimal` and optionally `rust_decimal_macros` to your `Cargo.toml`:
//!
//! ```toml
//! [dependencies]
//! rust_decimal = "1.14"
//! rust_decimal_macros = "1.14"
//! ```
//!
//! ## Usage
//!
//! Decimal numbers can be created in a few distinct ways. The easiest and most optimal
//! method of creating a Decimal is to use the procedural macro within the
//! `rust_decimal_macros` crate:
//!
//! ```ignore
//! // Procedural macros need importing directly
//! use rust_decimal_macros::dec;
//!
//! let number = dec!(-1.23);
//! assert_eq!("-1.23", number.to_string());
//! ```
//!
//! Alternatively you can also use one of the Decimal number convenience functions:
//!
//! ```rust
//! // Using the prelude can help importing trait based functions (e.g. core::str::FromStr).
//! use rust_decimal::prelude::*;
//!
//! // Using an integer followed by the decimal points
//! let scaled = Decimal::new(202, 2);
//! assert_eq!("2.02", scaled.to_string());
//!
//! // From a string representation
//! let from_string = Decimal::from_str("2.02").unwrap();
//! assert_eq!("2.02", from_string.to_string());
//!
//! // From a string representation in a different base
//! let from_string_base16 = Decimal::from_str_radix("ffff", 16).unwrap();
//! assert_eq!("65535", from_string_base16.to_string());
//!
//! // Using the `Into` trait
//! let my_int: Decimal = 3i32.into();
//! assert_eq!("3", my_int.to_string());
//!
//! // Using the raw decimal representation
//! let pi = Decimal::from_parts(1102470952, 185874565, 1703060790, false, 28);
//! assert_eq!("3.1415926535897932384626433832", pi.to_string());
//! ```
//!
//! Once you have instantiated your `Decimal` number you can perform calculations with it just like any other number:
//!
//! ```rust
//! use rust_decimal::prelude::*;
//!
//! let amount = Decimal::from_str("25.12").unwrap();
//! let tax = Decimal::from_str("0.085").unwrap();
//! let total = amount + (amount * tax).round_dp(2);
//! assert_eq!(total.to_string(), "27.26");
//! ```
//!
//! ## Features
//!
//! * [c-repr](#c-repr)
//! * [db-postgres](#db-postgres)
//! * [db-tokio-postgres](#db-tokio-postgres)
//! * [db-diesel-postgres](#db-diesel-postgres)
//! * [legacy-ops](#legacy-ops)
//! * [maths](#maths)
//! * [rust-fuzz](#rust-fuzz)
//! * [serde-float](#serde-float)
//! * [serde-str](#serde-str)
//! * [serde-arbitrary-precision](#serde-arbitrary-precision)
//! * [std](#std)
//!
//! ## `c-repr`
//!
//! Forces `Decimal` to use `[repr(C)]`. The corresponding target layout is 128 bit aligned.
//!
//! ## `db-postgres`
//!
//! This feature enables a PostgreSQL communication module. It allows for reading and writing the `Decimal`
//! type by transparently serializing/deserializing into the `NUMERIC` data type within PostgreSQL.
//!
//! ## `db-tokio-postgres`
//!
//! Enables the tokio postgres module allowing for async communication with PostgreSQL.
//!
//! ## `db-diesel-postgres`
//!
//! Enable `diesel` PostgreSQL support.
//!
//! ## `legacy-ops`
//!
//! As of `1.10` the algorithms used to perform basic operations have changed which has benefits of significant speed improvements.
//! To maintain backwards compatibility this can be opted out of by enabling the `legacy-ops` feature.
//!
//! ## `maths`
//!
//! The `maths` feature enables additional complex mathematical functions such as `pow`, `ln`, `enf`, `exp` etc.
//! Documentation detailing the additional functions can be found on the
//! [`MathematicalOps`](https://docs.rs/rust_decimal/latest/rust_decimal/trait.MathematicalOps.html) trait.  
//!
//! ## `rust-fuzz`
//!
//! Enable `rust-fuzz` support by implementing the `Arbitrary` trait.
//!
//! ## `serde-float`
//!
//! Enable this so that JSON serialization of `Decimal` types are sent as a float instead of a string (default).
//!
//! e.g. with this turned on, JSON serialization would output:
//! ```json
//! {
//!   "value": 1.234
//! }
//! ```
//!
//! ## `serde-str`
//!
//! This is typically useful for `bincode` or `csv` like implementations.
//!
//! Since `bincode` does not specify type information, we need to ensure that a type hint is provided in order to
//! correctly be able to deserialize. Enabling this feature on its own will force deserialization to use `deserialize_str`
//! instead of `deserialize_any`.
//!
//! If, for some reason, you also have `serde-float` enabled then this will use `deserialize_f64` as a type hint. Because
//! converting to `f64` _loses_ precision, it's highly recommended that you do NOT enable this feature when working with
//! `bincode`. That being said, this will only use 8 bytes so is slightly more efficient in terms of storage size.
//!
//! ## `serde-arbitrary-precision`
//!
//! This is used primarily with `serde_json` and consequently adds it as a "weak dependency". This supports the
//! `arbitrary_precision` feature inside `serde_json` when parsing decimals.
//!
//! This is recommended when parsing "float" looking data as it will prevent data loss.
//!
//! ## `std`
//!
//! Enable `std` library support. This is enabled by default, however in the future will be opt in. For now, to support `no_std`
//! libraries, this crate can be compiled with `--no-default-features`.
//!
#![forbid(unsafe_code)]
#![cfg_attr(not(feature = "std"), no_std)]
extern crate alloc;

mod constants;
mod decimal;
mod error;
mod ops;
mod str;

#[cfg(any(feature = "postgres", feature = "diesel"))]
mod db;
#[cfg(feature = "rust-fuzz")]
mod fuzz;
#[cfg(feature = "maths")]
mod maths;
#[cfg(feature = "serde")]
mod serde;

pub use decimal::{Decimal, RoundingStrategy};
pub use error::Error;
#[cfg(feature = "maths")]
pub use maths::MathematicalOps;

/// A convenience module appropriate for glob imports (`use rust_decimal::prelude::*;`).
pub mod prelude {
    #[cfg(feature = "maths")]
    pub use crate::maths::MathematicalOps;
    pub use crate::{Decimal, RoundingStrategy};
    pub use core::str::FromStr;
    pub use num_traits::{FromPrimitive, One, ToPrimitive, Zero};
}

#[cfg(feature = "diesel")]
#[macro_use]
extern crate diesel;

/// Shortcut for `core::result::Result<T, rust_decimal::Error>`. Useful to distinguish
/// between `rust_decimal` and `std` types.
pub type Result<T> = core::result::Result<T, Error>;
