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
#![doc(html_logo_url = "https://rust-num.github.io/num/rust-logo-128x128-blk-v2.png",
       html_favicon_url = "https://rust-num.github.io/num/favicon.ico",
       html_root_url = "https://rust-num.github.io/num/",
       html_playground_url = "http://play.integer32.com/")]

use std::ops::{Add, Sub, Mul, Div, Rem};
use std::ops::{AddAssign, SubAssign, MulAssign, DivAssign, RemAssign};
use std::num::Wrapping;

pub use bounds::Bounded;
pub use float::{Float, FloatConst};
pub use identities::{Zero, One, zero, one};
pub use ops::checked::*;
pub use ops::wrapping::*;
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

/// The base trait for numeric types, covering `0` and `1` values,
/// comparisons, basic numeric operations, and string conversion.
pub trait Num: PartialEq + Zero + One + NumOps
{
    type FromStrRadixErr;

    /// Convert from a string and radix <= 36.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use num_traits::Num;
    ///
    /// let result = <i32 as Num>::from_str_radix("27", 10);
    /// assert_eq!(result, Ok(27));
    ///
    /// let result = <i32 as Num>::from_str_radix("foo", 10);
    /// assert!(result.is_err());
    /// ```
    fn from_str_radix(str: &str, radix: u32) -> Result<Self, Self::FromStrRadixErr>;
}

/// The trait for types implementing basic numeric operations
///
/// This is automatically implemented for types which implement the operators.
pub trait NumOps<Rhs = Self, Output = Self>
    : Add<Rhs, Output = Output>
    + Sub<Rhs, Output = Output>
    + Mul<Rhs, Output = Output>
    + Div<Rhs, Output = Output>
    + Rem<Rhs, Output = Output>
{}

impl<T, Rhs, Output> NumOps<Rhs, Output> for T
where T: Add<Rhs, Output = Output>
       + Sub<Rhs, Output = Output>
       + Mul<Rhs, Output = Output>
       + Div<Rhs, Output = Output>
       + Rem<Rhs, Output = Output>
{}

/// The trait for `Num` types which also implement numeric operations taking
/// the second operand by reference.
///
/// This is automatically implemented for types which implement the operators.
pub trait NumRef: Num + for<'r> NumOps<&'r Self> {}
impl<T> NumRef for T where T: Num + for<'r> NumOps<&'r T> {}

/// The trait for references which implement numeric operations, taking the
/// second operand either by value or by reference.
///
/// This is automatically implemented for types which implement the operators.
pub trait RefNum<Base>: NumOps<Base, Base> + for<'r> NumOps<&'r Base, Base> {}
impl<T, Base> RefNum<Base> for T where T: NumOps<Base, Base> + for<'r> NumOps<&'r Base, Base> {}

/// The trait for types implementing numeric assignment operators (like `+=`).
///
/// This is automatically implemented for types which implement the operators.
pub trait NumAssignOps<Rhs = Self>
    : AddAssign<Rhs>
    + SubAssign<Rhs>
    + MulAssign<Rhs>
    + DivAssign<Rhs>
    + RemAssign<Rhs>
{}

impl<T, Rhs> NumAssignOps<Rhs> for T
where T: AddAssign<Rhs>
       + SubAssign<Rhs>
       + MulAssign<Rhs>
       + DivAssign<Rhs>
       + RemAssign<Rhs>
{}

/// The trait for `Num` types which also implement assignment operators.
///
/// This is automatically implemented for types which implement the operators.
pub trait NumAssign: Num + NumAssignOps {}
impl<T> NumAssign for T where T: Num + NumAssignOps {}

/// The trait for `NumAssign` types which also implement assignment operations
/// taking the second operand by reference.
///
/// This is automatically implemented for types which implement the operators.
pub trait NumAssignRef: NumAssign + for<'r> NumAssignOps<&'r Self> {}
impl<T> NumAssignRef for T where T: NumAssign + for<'r> NumAssignOps<&'r T> {}


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

impl<T: Num> Num for Wrapping<T>
    where Wrapping<T>:
          Add<Output = Wrapping<T>> + Sub<Output = Wrapping<T>>
        + Mul<Output = Wrapping<T>> + Div<Output = Wrapping<T>> + Rem<Output = Wrapping<T>>
{
    type FromStrRadixErr = T::FromStrRadixErr;
    fn from_str_radix(str: &str, radix: u32) -> Result<Self, Self::FromStrRadixErr> {
        T::from_str_radix(str, radix).map(Wrapping)
    }
}


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

/// A value bounded by a minimum and a maximum
///
///  If input is less than min then this returns min.
///  If input is greater than max then this returns max.
///  Otherwise this returns input.
#[inline]
pub fn clamp<T: PartialOrd>(input: T, min: T, max: T) -> T {
    debug_assert!(min <= max, "min must be less than or equal to max");
    if input < min {
        min
    } else if input > max {
        max
    } else {
        input
    }
}

#[test]
fn clamp_test() {
    // Int test
    assert_eq!(1, clamp(1, -1, 2));
    assert_eq!(-1, clamp(-2, -1, 2));
    assert_eq!(2, clamp(3, -1, 2));

    // Float test
    assert_eq!(1.0, clamp(1.0, -1.0, 2.0));
    assert_eq!(-1.0, clamp(-2.0, -1.0, 2.0));
    assert_eq!(2.0, clamp(3.0, -1.0, 2.0));
}

#[test]
fn from_str_radix_unwrap() {
    // The Result error must impl Debug to allow unwrap()

    let i: i32 = Num::from_str_radix("0", 10).unwrap();
    assert_eq!(i, 0);

    let f: f32 = Num::from_str_radix("0.0", 10).unwrap();
    assert_eq!(f, 0.0);
}

#[test]
fn wrapping_is_num() {
    fn require_num<T: Num>(_: &T) {}
    require_num(&Wrapping(42_u32));
    require_num(&Wrapping(-42));
}

#[test]
fn wrapping_from_str_radix() {
    macro_rules! test_wrapping_from_str_radix {
        ($($t:ty)+) => {
            $(
                for &(s, r) in &[("42", 10), ("42", 2), ("-13.0", 10), ("foo", 10)] {
                    let w = Wrapping::<$t>::from_str_radix(s, r).map(|w| w.0);
                    assert_eq!(w, <$t as Num>::from_str_radix(s, r));
                }
            )+
        };
    }

    test_wrapping_from_str_radix!(usize u8 u16 u32 u64 isize i8 i16 i32 i64);
}

#[test]
fn check_num_ops() {
    fn compute<T: Num + Copy>(x: T, y: T) -> T {
        x * y / y % y + y - y
    }
    assert_eq!(compute(1, 2), 1)
}

#[test]
fn check_numref_ops() {
    fn compute<T: NumRef>(x: T, y: &T) -> T {
        x * y / y % y + y - y
    }
    assert_eq!(compute(1, &2), 1)
}

#[test]
fn check_refnum_ops() {
    fn compute<T: Copy>(x: &T, y: T) -> T
        where for<'a> &'a T: RefNum<T>
    {
        &(&(&(&(x * y) / y) % y) + y) - y
    }
    assert_eq!(compute(&1, 2), 1)
}

#[test]
fn check_refref_ops() {
    fn compute<T>(x: &T, y: &T) -> T
        where for<'a> &'a T: RefNum<T>
    {
        &(&(&(&(x * y) / y) % y) + y) - y
    }
    assert_eq!(compute(&1, &2), 1)
}

#[test]
fn check_numassign_ops() {
    fn compute<T: NumAssign + Copy>(mut x: T, y: T) -> T {
        x *= y;
        x /= y;
        x %= y;
        x += y;
        x -= y;
        x
    }
    assert_eq!(compute(1, 2), 1)
}

// TODO test `NumAssignRef`, but even the standard numeric types don't
// implement this yet. (see rust pr41336)
