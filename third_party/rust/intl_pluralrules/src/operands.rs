//! Plural operands in compliance with [CLDR Plural Rules](https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules).
//!
//! See [full operands description](https://unicode.org/reports/tr35/tr35-numbers.html#Operands).
//!
//! # Examples
//!
//! From int
//!
//! ```
//! use std::convert::TryFrom;
//! use intl_pluralrules::operands::*;
//! assert_eq!(Ok(PluralOperands {
//!    n: 2_f64,
//!    i: 2,
//!    v: 0,
//!    w: 0,
//!    f: 0,
//!    t: 0,
//! }), PluralOperands::try_from(2))
//! ```
//!
//! From float
//!
//! ```
//! use std::convert::TryFrom;
//! use intl_pluralrules::operands::*;
//! assert_eq!(Ok(PluralOperands {
//!    n: 1234.567_f64,
//!    i: 1234,
//!    v: 3,
//!    w: 3,
//!    f: 567,
//!    t: 567,
//! }), PluralOperands::try_from("-1234.567"))
//! ```
//!
//! From &str
//!
//! ```
//! use std::convert::TryFrom;
//! use intl_pluralrules::operands::*;
//! assert_eq!(Ok(PluralOperands {
//!    n: 123.45_f64,
//!    i: 123,
//!    v: 2,
//!    w: 2,
//!    f: 45,
//!    t: 45,
//! }), PluralOperands::try_from(123.45))
//! ```
#![cfg_attr(feature = "cargo-clippy", allow(clippy::cast_lossless))]
use std::convert::TryFrom;
use std::isize;
use std::str::FromStr;

/// A full plural operands representation of a number. See [CLDR Plural Rules](https://unicode.org/reports/tr35/tr35-numbers.html#Language_Plural_Rules) for complete operands description.
#[derive(Debug, PartialEq)]
pub struct PluralOperands {
    /// Absolute value of input
    pub n: f64,
    /// Integer value of input
    pub i: u64,
    /// Number of visible fraction digits with trailing zeros
    pub v: usize,
    /// Number of visible fraction digits without trailing zeros
    pub w: usize,
    /// Visible fraction digits with trailing zeros
    pub f: u64,
    /// Visible fraction digits without trailing zeros
    pub t: u64,
}

impl<'a> TryFrom<&'a str> for PluralOperands {
    type Error = &'static str;

    fn try_from(input: &'a str) -> Result<Self, Self::Error> {
        let abs_str = if input.starts_with('-') {
            &input[1..]
        } else {
            &input
        };

        let absolute_value = f64::from_str(&abs_str).map_err(|_| "Incorrect number passed!")?;

        let integer_digits;
        let num_fraction_digits0;
        let num_fraction_digits;
        let fraction_digits0;
        let fraction_digits;

        if let Some(dec_pos) = abs_str.find('.') {
            let int_str = &abs_str[..dec_pos];
            let dec_str = &abs_str[(dec_pos + 1)..];

            integer_digits =
                u64::from_str(&int_str).map_err(|_| "Could not convert string to integer!")?;

            let backtrace = dec_str.trim_end_matches('0');

            num_fraction_digits0 = dec_str.len() as usize;
            num_fraction_digits = backtrace.len() as usize;
            fraction_digits0 =
                u64::from_str(dec_str).map_err(|_| "Could not convert string to integer!")?;
            fraction_digits = u64::from_str(backtrace).unwrap_or(0);
        } else {
            integer_digits = absolute_value as u64;
            num_fraction_digits0 = 0;
            num_fraction_digits = 0;
            fraction_digits0 = 0;
            fraction_digits = 0;
        }

        Ok(PluralOperands {
            n: absolute_value,
            i: integer_digits,
            v: num_fraction_digits0,
            w: num_fraction_digits,
            f: fraction_digits0,
            t: fraction_digits,
        })
    }
}

macro_rules! impl_integer_type {
    ($ty:ident) => {
        impl From<$ty> for PluralOperands {
            fn from(input: $ty) -> Self {
                // XXXManishearth converting from u32 or u64 to isize may wrap
                PluralOperands {
                    n: input as f64,
                    i: input as u64,
                    v: 0,
                    w: 0,
                    f: 0,
                    t: 0,
                }
            }
        }
    };
    ($($ty:ident)+) => {
        $(impl_integer_type!($ty);)+
    };
}

macro_rules! impl_signed_integer_type {
    ($ty:ident) => {
        impl TryFrom<$ty> for PluralOperands {
            type Error = &'static str;
            fn try_from(input: $ty) -> Result<Self, Self::Error> {
                // XXXManishearth converting from i64 to isize may wrap
                let x = (input as isize).checked_abs().ok_or("Number too big")?;
                Ok(PluralOperands {
                    n: x as f64,
                    i: x as u64,
                    v: 0,
                    w: 0,
                    f: 0,
                    t: 0,
                })
            }
        }
    };
    ($($ty:ident)+) => {
        $(impl_signed_integer_type!($ty);)+
    };
}

macro_rules! impl_convert_type {
    ($ty:ident) => {
        impl TryFrom<$ty> for PluralOperands {
            type Error = &'static str;
            fn try_from(input: $ty) -> Result<Self, Self::Error> {
                let as_str: &str = &input.to_string();
                PluralOperands::try_from(as_str)
            }
        }
    };
    ($($ty:ident)+) => {
        $(impl_convert_type!($ty);)+
    };
}

impl_integer_type!(u8 u16 u32 u64 usize);
impl_signed_integer_type!(i8 i16 i32 i64 isize);
// XXXManishearth we can likely have dedicated float impls here
impl_convert_type!(f32 f64 String);
