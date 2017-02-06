// Copyright 2013-2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! Numeric traits for generic mathematics

use std::ops::{Add, Sub, Mul, Div, Rem};

pub use bounds::Bounded;
pub use float::{Float, FloatConst};
pub use identities::{Zero, One, zero, one};
pub use ops::checked::*;
pub use ops::saturating::Saturating;
pub use sign::{Signed, Unsigned, abs, abs_sub, signum};
pub use cast::*;
pub use int::PrimInt;
pub use pow::{pow, checked_pow};

pub mod identities;
pub mod sign;
pub mod ops;
pub mod bounds;
pub mod float;
pub mod cast;
pub mod int;
pub mod pow;

/// The base trait for numeric types
pub trait Num: PartialEq + Zero + One
    + Add<Output = Self> + Sub<Output = Self>
    + Mul<Output = Self> + Div<Output = Self> + Rem<Output = Self>
{
    type FromStrRadixErr;

    /// Convert from a string and radix <= 36.
    fn from_str_radix(str: &str, radix: u32) -> Result<Self, Self::FromStrRadixErr>;
}

macro_rules! int_trait_impl {
    ($name:ident for $($t:ty)*) => ($(
        impl $name for $t {
            type FromStrRadixErr = ::std::num::ParseIntError;
            #[inline]
            fn from_str_radix(s: &str, radix: u32)
                              -> Result<Self, ::std::num::ParseIntError>
            {
                <$t>::from_str_radix(s, radix)
            }
        }
    )*)
}
int_trait_impl!(Num for usize u8 u16 u32 u64 isize i8 i16 i32 i64);

#[derive(Debug)]
pub enum FloatErrorKind {
    Empty,
    Invalid,
}
// FIXME: std::num::ParseFloatError is stable in 1.0, but opaque to us,
// so there's not really any way for us to reuse it.
#[derive(Debug)]
pub struct ParseFloatError {
    pub kind: FloatErrorKind,
}

// FIXME: The standard library from_str_radix on floats was deprecated, so we're stuck
// with this implementation ourselves until we want to make a breaking change.
// (would have to drop it from `Num` though)
macro_rules! float_trait_impl {
    ($name:ident for $($t:ty)*) => ($(
        impl $name for $t {
            type FromStrRadixErr = ParseFloatError;

            fn from_str_radix(src: &str, radix: u32)
                              -> Result<Self, Self::FromStrRadixErr>
            {
                use self::FloatErrorKind::*;
                use self::ParseFloatError as PFE;

                // Special values
                match src {
                    "inf"   => return Ok(Float::infinity()),
                    "-inf"  => return Ok(Float::neg_infinity()),
                    "NaN"   => return Ok(Float::nan()),
                    _       => {},
                }

                fn slice_shift_char(src: &str) -> Option<(char, &str)> {
                    src.chars().nth(0).map(|ch| (ch, &src[1..]))
                }

                let (is_positive, src) =  match slice_shift_char(src) {
                    None             => return Err(PFE { kind: Empty }),
                    Some(('-', ""))  => return Err(PFE { kind: Empty }),
                    Some(('-', src)) => (false, src),
                    Some((_, _))     => (true,  src),
                };

                // The significand to accumulate
                let mut sig = if is_positive { 0.0 } else { -0.0 };
                // Necessary to detect overflow
                let mut prev_sig = sig;
                let mut cs = src.chars().enumerate();
                // Exponent prefix and exponent index offset
                let mut exp_info = None::<(char, usize)>;

                // Parse the integer part of the significand
                for (i, c) in cs.by_ref() {
                    match c.to_digit(radix) {
                        Some(digit) => {
                            // shift significand one digit left
                            sig = sig * (radix as $t);

                            // add/subtract current digit depending on sign
                            if is_positive {
                                sig = sig + ((digit as isize) as $t);
                            } else {
                                sig = sig - ((digit as isize) as $t);
                            }

                            // Detect overflow by comparing to last value, except
                            // if we've not seen any non-zero digits.
                            if prev_sig != 0.0 {
                                if is_positive && sig <= prev_sig
                                    { return Ok(Float::infinity()); }
                                if !is_positive && sig >= prev_sig
                                    { return Ok(Float::neg_infinity()); }

                                // Detect overflow by reversing the shift-and-add process
                                if is_positive && (prev_sig != (sig - digit as $t) / radix as $t)
                                    { return Ok(Float::infinity()); }
                                if !is_positive && (prev_sig != (sig + digit as $t) / radix as $t)
                                    { return Ok(Float::neg_infinity()); }
                            }
                            prev_sig = sig;
                        },
                        None => match c {
                            'e' | 'E' | 'p' | 'P' => {
                                exp_info = Some((c, i + 1));
                                break;  // start of exponent
                            },
                            '.' => {
                                break;  // start of fractional part
                            },
                            _ => {
                                return Err(PFE { kind: Invalid });
                            },
                        },
                    }
                }

                // If we are not yet at the exponent parse the fractional
                // part of the significand
                if exp_info.is_none() {
                    let mut power = 1.0;
                    for (i, c) in cs.by_ref() {
                        match c.to_digit(radix) {
                            Some(digit) => {
                                // Decrease power one order of magnitude
                                power = power / (radix as $t);
                                // add/subtract current digit depending on sign
                                sig = if is_positive {
                                    sig + (digit as $t) * power
                                } else {
                                    sig - (digit as $t) * power
                                };
                                // Detect overflow by comparing to last value
                                if is_positive && sig < prev_sig
                                    { return Ok(Float::infinity()); }
                                if !is_positive && sig > prev_sig
                                    { return Ok(Float::neg_infinity()); }
                                prev_sig = sig;
                            },
                            None => match c {
                                'e' | 'E' | 'p' | 'P' => {
                                    exp_info = Some((c, i + 1));
                                    break; // start of exponent
                                },
                                _ => {
                                    return Err(PFE { kind: Invalid });
                                },
                            },
                        }
                    }
                }

                // Parse and calculate the exponent
                let exp = match exp_info {
                    Some((c, offset)) => {
                        let base = match c {
                            'E' | 'e' if radix == 10 => 10.0,
                            'P' | 'p' if radix == 16 => 2.0,
                            _ => return Err(PFE { kind: Invalid }),
                        };

                        // Parse the exponent as decimal integer
                        let src = &src[offset..];
                        let (is_positive, exp) = match slice_shift_char(src) {
                            Some(('-', src)) => (false, src.parse::<usize>()),
                            Some(('+', src)) => (true,  src.parse::<usize>()),
                            Some((_, _))     => (true,  src.parse::<usize>()),
                            None             => return Err(PFE { kind: Invalid }),
                        };

                        match (is_positive, exp) {
                            (true,  Ok(exp)) => base.powi(exp as i32),
                            (false, Ok(exp)) => 1.0 / base.powi(exp as i32),
                            (_, Err(_))      => return Err(PFE { kind: Invalid }),
                        }
                    },
                    None => 1.0, // no exponent
                };

                Ok(sig * exp)
            }
        }
    )*)
}
float_trait_impl!(Num for f32 f64);

#[test]
fn from_str_radix_unwrap() {
    // The Result error must impl Debug to allow unwrap()

    let i: i32 = Num::from_str_radix("0", 10).unwrap();
    assert_eq!(i, 0);

    let f: f32 = Num::from_str_radix("0.0", 10).unwrap();
    assert_eq!(f, 0.0);
}
