use std::default::Default;
use std::ops::{Add, Div, Mul, Neg, Rem, Shl, Shr, Sub, Not};
use std::str::{self, FromStr};
use std::fmt;
use std::cmp::Ordering::{self, Less, Greater, Equal};
use std::{i64, u64};
#[allow(unused_imports)]
use std::ascii::AsciiExt;

#[cfg(feature = "serde")]
use serde;

// Some of the tests of non-RNG-based functionality are randomized using the
// RNG-based functionality, so the RNG-based functionality needs to be enabled
// for tests.
#[cfg(any(feature = "rand", test))]
use rand::Rng;

use integer::Integer;
use traits::{ToPrimitive, FromPrimitive, Num, CheckedAdd, CheckedSub,
             CheckedMul, CheckedDiv, Signed, Zero, One};

use self::Sign::{Minus, NoSign, Plus};

use super::ParseBigIntError;
use super::big_digit;
use super::big_digit::{BigDigit, DoubleBigDigit};
use biguint;
use biguint::to_str_radix_reversed;
use biguint::BigUint;

use UsizePromotion;
use IsizePromotion;

#[cfg(test)]
#[path = "tests/bigint.rs"]
mod bigint_tests;

/// A Sign is a `BigInt`'s composing element.
#[derive(PartialEq, PartialOrd, Eq, Ord, Copy, Clone, Debug, Hash)]
#[cfg_attr(feature = "rustc-serialize", derive(RustcEncodable, RustcDecodable))]
pub enum Sign {
    Minus,
    NoSign,
    Plus,
}

impl Neg for Sign {
    type Output = Sign;

    /// Negate Sign value.
    #[inline]
    fn neg(self) -> Sign {
        match self {
            Minus => Plus,
            NoSign => NoSign,
            Plus => Minus,
        }
    }
}

impl Mul<Sign> for Sign {
    type Output = Sign;

    #[inline]
    fn mul(self, other: Sign) -> Sign {
        match (self, other) {
            (NoSign, _) | (_, NoSign) => NoSign,
            (Plus, Plus) | (Minus, Minus) => Plus,
            (Plus, Minus) | (Minus, Plus) => Minus,
        }
    }
}

#[cfg(feature = "serde")]
impl serde::Serialize for Sign {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
        where S: serde::Serializer
    {
        match *self {
            Sign::Minus => (-1i8).serialize(serializer),
            Sign::NoSign => 0i8.serialize(serializer),
            Sign::Plus => 1i8.serialize(serializer),
        }
    }
}

#[cfg(feature = "serde")]
impl serde::Deserialize for Sign {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
        where D: serde::Deserializer
    {
        use serde::de::Error;

        let sign: i8 = try!(serde::Deserialize::deserialize(deserializer));
        match sign {
            -1 => Ok(Sign::Minus),
            0 => Ok(Sign::NoSign),
            1 => Ok(Sign::Plus),
            _ => Err(D::Error::invalid_value("sign must be -1, 0, or 1")),
        }
    }
}

/// A big signed integer type.
#[derive(Clone, Debug, Hash)]
#[cfg_attr(feature = "rustc-serialize", derive(RustcEncodable, RustcDecodable))]
pub struct BigInt {
    sign: Sign,
    data: BigUint,
}

impl PartialEq for BigInt {
    #[inline]
    fn eq(&self, other: &BigInt) -> bool {
        self.cmp(other) == Equal
    }
}

impl Eq for BigInt {}

impl PartialOrd for BigInt {
    #[inline]
    fn partial_cmp(&self, other: &BigInt) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for BigInt {
    #[inline]
    fn cmp(&self, other: &BigInt) -> Ordering {
        let scmp = self.sign.cmp(&other.sign);
        if scmp != Equal {
            return scmp;
        }

        match self.sign {
            NoSign => Equal,
            Plus => self.data.cmp(&other.data),
            Minus => other.data.cmp(&self.data),
        }
    }
}

impl Default for BigInt {
    #[inline]
    fn default() -> BigInt {
        Zero::zero()
    }
}

impl fmt::Display for BigInt {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.pad_integral(!self.is_negative(), "", &self.data.to_str_radix(10))
    }
}

impl fmt::Binary for BigInt {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.pad_integral(!self.is_negative(), "0b", &self.data.to_str_radix(2))
    }
}

impl fmt::Octal for BigInt {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.pad_integral(!self.is_negative(), "0o", &self.data.to_str_radix(8))
    }
}

impl fmt::LowerHex for BigInt {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.pad_integral(!self.is_negative(), "0x", &self.data.to_str_radix(16))
    }
}

impl fmt::UpperHex for BigInt {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.pad_integral(!self.is_negative(),
                       "0x",
                       &self.data.to_str_radix(16).to_ascii_uppercase())
    }
}

impl FromStr for BigInt {
    type Err = ParseBigIntError;

    #[inline]
    fn from_str(s: &str) -> Result<BigInt, ParseBigIntError> {
        BigInt::from_str_radix(s, 10)
    }
}

impl Num for BigInt {
    type FromStrRadixErr = ParseBigIntError;

    /// Creates and initializes a BigInt.
    #[inline]
    fn from_str_radix(mut s: &str, radix: u32) -> Result<BigInt, ParseBigIntError> {
        let sign = if s.starts_with('-') {
            let tail = &s[1..];
            if !tail.starts_with('+') {
                s = tail
            }
            Minus
        } else {
            Plus
        };
        let bu = try!(BigUint::from_str_radix(s, radix));
        Ok(BigInt::from_biguint(sign, bu))
    }
}

impl Shl<usize> for BigInt {
    type Output = BigInt;

    #[inline]
    fn shl(self, rhs: usize) -> BigInt {
        (&self) << rhs
    }
}

impl<'a> Shl<usize> for &'a BigInt {
    type Output = BigInt;

    #[inline]
    fn shl(self, rhs: usize) -> BigInt {
        BigInt::from_biguint(self.sign, &self.data << rhs)
    }
}

impl Shr<usize> for BigInt {
    type Output = BigInt;

    #[inline]
    fn shr(self, rhs: usize) -> BigInt {
        BigInt::from_biguint(self.sign, self.data >> rhs)
    }
}

impl<'a> Shr<usize> for &'a BigInt {
    type Output = BigInt;

    #[inline]
    fn shr(self, rhs: usize) -> BigInt {
        BigInt::from_biguint(self.sign, &self.data >> rhs)
    }
}

impl Zero for BigInt {
    #[inline]
    fn zero() -> BigInt {
        BigInt::from_biguint(NoSign, Zero::zero())
    }

    #[inline]
    fn is_zero(&self) -> bool {
        self.sign == NoSign
    }
}

impl One for BigInt {
    #[inline]
    fn one() -> BigInt {
        BigInt::from_biguint(Plus, One::one())
    }
}

impl Signed for BigInt {
    #[inline]
    fn abs(&self) -> BigInt {
        match self.sign {
            Plus | NoSign => self.clone(),
            Minus => BigInt::from_biguint(Plus, self.data.clone()),
        }
    }

    #[inline]
    fn abs_sub(&self, other: &BigInt) -> BigInt {
        if *self <= *other {
            Zero::zero()
        } else {
            self - other
        }
    }

    #[inline]
    fn signum(&self) -> BigInt {
        match self.sign {
            Plus => BigInt::from_biguint(Plus, One::one()),
            Minus => BigInt::from_biguint(Minus, One::one()),
            NoSign => Zero::zero(),
        }
    }

    #[inline]
    fn is_positive(&self) -> bool {
        self.sign == Plus
    }

    #[inline]
    fn is_negative(&self) -> bool {
        self.sign == Minus
    }
}

// A convenience method for getting the absolute value of an i32 in a u32.
#[inline]
fn i32_abs_as_u32(a: i32) -> u32 {
    if a == i32::min_value() {
        a as u32
    } else {
        a.abs() as u32
    }
}

// A convenience method for getting the absolute value of an i64 in a u64.
#[inline]
fn i64_abs_as_u64(a: i64) -> u64 {
    if a == i64::min_value() {
        a as u64
    } else {
        a.abs() as u64
    }
}

// We want to forward to BigUint::add, but it's not clear how that will go until
// we compare both sign and magnitude.  So we duplicate this body for every
// val/ref combination, deferring that decision to BigUint's own forwarding.
macro_rules! bigint_add {
    ($a:expr, $a_owned:expr, $a_data:expr, $b:expr, $b_owned:expr, $b_data:expr) => {
        match ($a.sign, $b.sign) {
            (_, NoSign) => $a_owned,
            (NoSign, _) => $b_owned,
            // same sign => keep the sign with the sum of magnitudes
            (Plus, Plus) | (Minus, Minus) =>
                BigInt::from_biguint($a.sign, $a_data + $b_data),
            // opposite signs => keep the sign of the larger with the difference of magnitudes
            (Plus, Minus) | (Minus, Plus) =>
                match $a.data.cmp(&$b.data) {
                    Less => BigInt::from_biguint($b.sign, $b_data - $a_data),
                    Greater => BigInt::from_biguint($a.sign, $a_data - $b_data),
                    Equal => Zero::zero(),
                },
        }
    };
}

impl<'a, 'b> Add<&'b BigInt> for &'a BigInt {
    type Output = BigInt;

    #[inline]
    fn add(self, other: &BigInt) -> BigInt {
        bigint_add!(self,
                    self.clone(),
                    &self.data,
                    other,
                    other.clone(),
                    &other.data)
    }
}

impl<'a> Add<BigInt> for &'a BigInt {
    type Output = BigInt;

    #[inline]
    fn add(self, other: BigInt) -> BigInt {
        bigint_add!(self, self.clone(), &self.data, other, other, other.data)
    }
}

impl<'a> Add<&'a BigInt> for BigInt {
    type Output = BigInt;

    #[inline]
    fn add(self, other: &BigInt) -> BigInt {
        bigint_add!(self, self, self.data, other, other.clone(), &other.data)
    }
}

impl Add<BigInt> for BigInt {
    type Output = BigInt;

    #[inline]
    fn add(self, other: BigInt) -> BigInt {
        bigint_add!(self, self, self.data, other, other, other.data)
    }
}

promote_all_scalars!(impl Add for BigInt, add);
forward_all_scalar_binop_to_val_val_commutative!(impl Add<BigDigit> for BigInt, add);
forward_all_scalar_binop_to_val_val_commutative!(impl Add<DoubleBigDigit> for BigInt, add);

impl Add<BigDigit> for BigInt {
    type Output = BigInt;

    #[inline]
    fn add(self, other: BigDigit) -> BigInt {
        match self.sign {
            NoSign => From::from(other),
            Plus => BigInt::from_biguint(Plus, self.data + other),
            Minus =>
                match self.data.cmp(&From::from(other)) {
                    Equal => Zero::zero(),
                    Less => BigInt::from_biguint(Plus, other - self.data),
                    Greater => BigInt::from_biguint(Minus, self.data - other),
                }
        }
    }
}

impl Add<DoubleBigDigit> for BigInt {
    type Output = BigInt;

    #[inline]
    fn add(self, other: DoubleBigDigit) -> BigInt {
        match self.sign {
            NoSign => From::from(other),
            Plus => BigInt::from_biguint(Plus, self.data + other),
            Minus =>
                match self.data.cmp(&From::from(other)) {
                    Equal => Zero::zero(),
                    Less => BigInt::from_biguint(Plus, other - self.data),
                    Greater => BigInt::from_biguint(Minus, self.data - other),
                }
        }
    }
}

forward_all_scalar_binop_to_val_val_commutative!(impl Add<i32> for BigInt, add);
forward_all_scalar_binop_to_val_val_commutative!(impl Add<i64> for BigInt, add);

impl Add<i32> for BigInt {
    type Output = BigInt;

    #[inline]
    fn add(self, other: i32) -> BigInt {
        if other >= 0 {
            self + other as u32
        } else {
            self - i32_abs_as_u32(other)
        }
    }
}

impl Add<i64> for BigInt {
    type Output = BigInt;

    #[inline]
    fn add(self, other: i64) -> BigInt {
        if other >= 0 {
            self + other as u64
        } else {
            self - i64_abs_as_u64(other)
        }
    }
}

// We want to forward to BigUint::sub, but it's not clear how that will go until
// we compare both sign and magnitude.  So we duplicate this body for every
// val/ref combination, deferring that decision to BigUint's own forwarding.
macro_rules! bigint_sub {
    ($a:expr, $a_owned:expr, $a_data:expr, $b:expr, $b_owned:expr, $b_data:expr) => {
        match ($a.sign, $b.sign) {
            (_, NoSign) => $a_owned,
            (NoSign, _) => -$b_owned,
            // opposite signs => keep the sign of the left with the sum of magnitudes
            (Plus, Minus) | (Minus, Plus) =>
                BigInt::from_biguint($a.sign, $a_data + $b_data),
            // same sign => keep or toggle the sign of the left with the difference of magnitudes
            (Plus, Plus) | (Minus, Minus) =>
                match $a.data.cmp(&$b.data) {
                    Less => BigInt::from_biguint(-$a.sign, $b_data - $a_data),
                    Greater => BigInt::from_biguint($a.sign, $a_data - $b_data),
                    Equal => Zero::zero(),
                },
        }
    };
}

impl<'a, 'b> Sub<&'b BigInt> for &'a BigInt {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: &BigInt) -> BigInt {
        bigint_sub!(self,
                    self.clone(),
                    &self.data,
                    other,
                    other.clone(),
                    &other.data)
    }
}

impl<'a> Sub<BigInt> for &'a BigInt {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: BigInt) -> BigInt {
        bigint_sub!(self, self.clone(), &self.data, other, other, other.data)
    }
}

impl<'a> Sub<&'a BigInt> for BigInt {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: &BigInt) -> BigInt {
        bigint_sub!(self, self, self.data, other, other.clone(), &other.data)
    }
}

impl Sub<BigInt> for BigInt {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: BigInt) -> BigInt {
        bigint_sub!(self, self, self.data, other, other, other.data)
    }
}

promote_all_scalars!(impl Sub for BigInt, sub);
forward_all_scalar_binop_to_val_val!(impl Sub<BigDigit> for BigInt, sub);
forward_all_scalar_binop_to_val_val!(impl Sub<DoubleBigDigit> for BigInt, sub);

impl Sub<BigDigit> for BigInt {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: BigDigit) -> BigInt {
        match self.sign {
            NoSign => BigInt::from_biguint(Minus, From::from(other)),
            Minus => BigInt::from_biguint(Minus, self.data + other),
            Plus =>
                match self.data.cmp(&From::from(other)) {
                    Equal => Zero::zero(),
                    Greater => BigInt::from_biguint(Plus, self.data - other),
                    Less => BigInt::from_biguint(Minus, other - self.data),
                }
        }
    }
}

impl Sub<BigInt> for BigDigit {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: BigInt) -> BigInt {
        -(other - self)
    }
}

impl Sub<DoubleBigDigit> for BigInt {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: DoubleBigDigit) -> BigInt {
        match self.sign {
            NoSign => BigInt::from_biguint(Minus, From::from(other)),
            Minus => BigInt::from_biguint(Minus, self.data + other),
            Plus =>
                match self.data.cmp(&From::from(other)) {
                    Equal => Zero::zero(),
                    Greater => BigInt::from_biguint(Plus, self.data - other),
                    Less => BigInt::from_biguint(Minus, other - self.data),
                }
        }
    }
}

impl Sub<BigInt> for DoubleBigDigit {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: BigInt) -> BigInt {
        -(other - self)
    }
}

forward_all_scalar_binop_to_val_val!(impl Sub<i32> for BigInt, sub);
forward_all_scalar_binop_to_val_val!(impl Sub<i64> for BigInt, sub);

impl Sub<i32> for BigInt {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: i32) -> BigInt {
        if other >= 0 {
            self - other as u32
        } else {
            self + i32_abs_as_u32(other)
        }
    }
}

impl Sub<BigInt> for i32 {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: BigInt) -> BigInt {
        if self >= 0 {
            self as u32 - other
        } else {
            -other - i32_abs_as_u32(self)
        }
    }
}

impl Sub<i64> for BigInt {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: i64) -> BigInt {
        if other >= 0 {
            self - other as u64
        } else {
            self + i64_abs_as_u64(other)
        }
    }
}

impl Sub<BigInt> for i64 {
    type Output = BigInt;

    #[inline]
    fn sub(self, other: BigInt) -> BigInt {
        if self >= 0 {
            self as u64 - other
        } else {
            -other - i64_abs_as_u64(self)
        }
    }
}

forward_all_binop_to_ref_ref!(impl Mul for BigInt, mul);

impl<'a, 'b> Mul<&'b BigInt> for &'a BigInt {
    type Output = BigInt;

    #[inline]
    fn mul(self, other: &BigInt) -> BigInt {
        BigInt::from_biguint(self.sign * other.sign, &self.data * &other.data)
    }
}

promote_all_scalars!(impl Mul for BigInt, mul);
forward_all_scalar_binop_to_val_val_commutative!(impl Mul<BigDigit> for BigInt, mul);
forward_all_scalar_binop_to_val_val_commutative!(impl Mul<DoubleBigDigit> for BigInt, mul);

impl Mul<BigDigit> for BigInt {
    type Output = BigInt;

    #[inline]
    fn mul(self, other: BigDigit) -> BigInt {
        BigInt::from_biguint(self.sign, self.data * other)
    }
}

impl Mul<DoubleBigDigit> for BigInt {
    type Output = BigInt;

    #[inline]
    fn mul(self, other: DoubleBigDigit) -> BigInt {
        BigInt::from_biguint(self.sign, self.data * other)
    }
}

forward_all_scalar_binop_to_val_val_commutative!(impl Mul<i32> for BigInt, mul);
forward_all_scalar_binop_to_val_val_commutative!(impl Mul<i64> for BigInt, mul);

impl Mul<i32> for BigInt {
    type Output = BigInt;

    #[inline]
    fn mul(self, other: i32) -> BigInt {
        if other >= 0 {
            self * other as u32
        } else {
            -(self * i32_abs_as_u32(other))
        }
    }
}

impl Mul<i64> for BigInt {
    type Output = BigInt;

    #[inline]
    fn mul(self, other: i64) -> BigInt {
        if other >= 0 {
            self * other as u64
        } else {
            -(self * i64_abs_as_u64(other))
        }
    }
}

forward_all_binop_to_ref_ref!(impl Div for BigInt, div);

impl<'a, 'b> Div<&'b BigInt> for &'a BigInt {
    type Output = BigInt;

    #[inline]
    fn div(self, other: &BigInt) -> BigInt {
        let (q, _) = self.div_rem(other);
        q
    }
}

promote_all_scalars!(impl Div for BigInt, div);
forward_all_scalar_binop_to_val_val!(impl Div<BigDigit> for BigInt, div);
forward_all_scalar_binop_to_val_val!(impl Div<DoubleBigDigit> for BigInt, div);

impl Div<BigDigit> for BigInt {
    type Output = BigInt;

    #[inline]
    fn div(self, other: BigDigit) -> BigInt {
        BigInt::from_biguint(self.sign, self.data / other)
    }
}

impl Div<BigInt> for BigDigit {
    type Output = BigInt;

    #[inline]
    fn div(self, other: BigInt) -> BigInt {
        BigInt::from_biguint(other.sign, self / other.data)
    }
}

impl Div<DoubleBigDigit> for BigInt {
    type Output = BigInt;

    #[inline]
    fn div(self, other: DoubleBigDigit) -> BigInt {
        BigInt::from_biguint(self.sign, self.data / other)
    }
}

impl Div<BigInt> for DoubleBigDigit {
    type Output = BigInt;

    #[inline]
    fn div(self, other: BigInt) -> BigInt {
        BigInt::from_biguint(other.sign, self / other.data)
    }
}

forward_all_scalar_binop_to_val_val!(impl Div<i32> for BigInt, div);
forward_all_scalar_binop_to_val_val!(impl Div<i64> for BigInt, div);

impl Div<i32> for BigInt {
    type Output = BigInt;

    #[inline]
    fn div(self, other: i32) -> BigInt {
        if other >= 0 {
            self / other as u32
        } else {
            -(self / i32_abs_as_u32(other))
        }
    }
}

impl Div<BigInt> for i32 {
    type Output = BigInt;

    #[inline]
    fn div(self, other: BigInt) -> BigInt {
        if self >= 0 {
            self as u32 / other
        } else {
            -(i32_abs_as_u32(self) / other)
        }
    }
}

impl Div<i64> for BigInt {
    type Output = BigInt;

    #[inline]
    fn div(self, other: i64) -> BigInt {
        if other >= 0 {
            self / other as u64
        } else {
            -(self / i64_abs_as_u64(other))
        }
    }
}

impl Div<BigInt> for i64 {
    type Output = BigInt;

    #[inline]
    fn div(self, other: BigInt) -> BigInt {
        if self >= 0 {
            self as u64 / other
        } else {
            -(i64_abs_as_u64(self) / other)
        }
    }
}

forward_all_binop_to_ref_ref!(impl Rem for BigInt, rem);

impl<'a, 'b> Rem<&'b BigInt> for &'a BigInt {
    type Output = BigInt;

    #[inline]
    fn rem(self, other: &BigInt) -> BigInt {
        let (_, r) = self.div_rem(other);
        r
    }
}

promote_all_scalars!(impl Rem for BigInt, rem);
forward_all_scalar_binop_to_val_val!(impl Rem<BigDigit> for BigInt, rem);
forward_all_scalar_binop_to_val_val!(impl Rem<DoubleBigDigit> for BigInt, rem);

impl Rem<BigDigit> for BigInt {
    type Output = BigInt;

    #[inline]
    fn rem(self, other: BigDigit) -> BigInt {
        BigInt::from_biguint(self.sign, self.data % other)
    }
}

impl Rem<BigInt> for BigDigit {
    type Output = BigInt;

    #[inline]
    fn rem(self, other: BigInt) -> BigInt {
        BigInt::from_biguint(Plus, self % other.data)
    }
}

impl Rem<DoubleBigDigit> for BigInt {
    type Output = BigInt;

    #[inline]
    fn rem(self, other: DoubleBigDigit) -> BigInt {
        BigInt::from_biguint(self.sign, self.data % other)
    }
}

impl Rem<BigInt> for DoubleBigDigit {
    type Output = BigInt;

    #[inline]
    fn rem(self, other: BigInt) -> BigInt {
        BigInt::from_biguint(Plus, self % other.data)
    }
}

forward_all_scalar_binop_to_val_val!(impl Rem<i32> for BigInt, rem);
forward_all_scalar_binop_to_val_val!(impl Rem<i64> for BigInt, rem);

impl Rem<i32> for BigInt {
    type Output = BigInt;

    #[inline]
    fn rem(self, other: i32) -> BigInt {
        if other >= 0 {
            self % other as u32
        } else {
            self % i32_abs_as_u32(other)
        }
    }
}

impl Rem<BigInt> for i32 {
    type Output = BigInt;

    #[inline]
    fn rem(self, other: BigInt) -> BigInt {
        if self >= 0 {
            self as u32 % other
        } else {
            -(i32_abs_as_u32(self) % other)
        }
    }
}

impl Rem<i64> for BigInt {
    type Output = BigInt;

    #[inline]
    fn rem(self, other: i64) -> BigInt {
        if other >= 0 {
            self % other as u64
        } else {
            self % i64_abs_as_u64(other)
        }
    }
}

impl Rem<BigInt> for i64 {
    type Output = BigInt;

    #[inline]
    fn rem(self, other: BigInt) -> BigInt {
        if self >= 0 {
            self as u64 % other
        } else {
            -(i64_abs_as_u64(self) % other)
        }
    }
}

impl Neg for BigInt {
    type Output = BigInt;

    #[inline]
    fn neg(mut self) -> BigInt {
        self.sign = -self.sign;
        self
    }
}

impl<'a> Neg for &'a BigInt {
    type Output = BigInt;

    #[inline]
    fn neg(self) -> BigInt {
        -self.clone()
    }
}

impl CheckedAdd for BigInt {
    #[inline]
    fn checked_add(&self, v: &BigInt) -> Option<BigInt> {
        return Some(self.add(v));
    }
}

impl CheckedSub for BigInt {
    #[inline]
    fn checked_sub(&self, v: &BigInt) -> Option<BigInt> {
        return Some(self.sub(v));
    }
}

impl CheckedMul for BigInt {
    #[inline]
    fn checked_mul(&self, v: &BigInt) -> Option<BigInt> {
        return Some(self.mul(v));
    }
}

impl CheckedDiv for BigInt {
    #[inline]
    fn checked_div(&self, v: &BigInt) -> Option<BigInt> {
        if v.is_zero() {
            return None;
        }
        return Some(self.div(v));
    }
}

impl Integer for BigInt {
    #[inline]
    fn div_rem(&self, other: &BigInt) -> (BigInt, BigInt) {
        // r.sign == self.sign
        let (d_ui, r_ui) = self.data.div_mod_floor(&other.data);
        let d = BigInt::from_biguint(self.sign, d_ui);
        let r = BigInt::from_biguint(self.sign, r_ui);
        if other.is_negative() {
            (-d, r)
        } else {
            (d, r)
        }
    }

    #[inline]
    fn div_floor(&self, other: &BigInt) -> BigInt {
        let (d, _) = self.div_mod_floor(other);
        d
    }

    #[inline]
    fn mod_floor(&self, other: &BigInt) -> BigInt {
        let (_, m) = self.div_mod_floor(other);
        m
    }

    fn div_mod_floor(&self, other: &BigInt) -> (BigInt, BigInt) {
        // m.sign == other.sign
        let (d_ui, m_ui) = self.data.div_rem(&other.data);
        let d = BigInt::from_biguint(Plus, d_ui);
        let m = BigInt::from_biguint(Plus, m_ui);
        let one: BigInt = One::one();
        match (self.sign, other.sign) {
            (_, NoSign) => panic!(),
            (Plus, Plus) | (NoSign, Plus) => (d, m),
            (Plus, Minus) | (NoSign, Minus) => {
                if m.is_zero() {
                    (-d, Zero::zero())
                } else {
                    (-d - one, m + other)
                }
            }
            (Minus, Plus) => {
                if m.is_zero() {
                    (-d, Zero::zero())
                } else {
                    (-d - one, other - m)
                }
            }
            (Minus, Minus) => (d, -m),
        }
    }

    /// Calculates the Greatest Common Divisor (GCD) of the number and `other`.
    ///
    /// The result is always positive.
    #[inline]
    fn gcd(&self, other: &BigInt) -> BigInt {
        BigInt::from_biguint(Plus, self.data.gcd(&other.data))
    }

    /// Calculates the Lowest Common Multiple (LCM) of the number and `other`.
    #[inline]
    fn lcm(&self, other: &BigInt) -> BigInt {
        BigInt::from_biguint(Plus, self.data.lcm(&other.data))
    }

    /// Deprecated, use `is_multiple_of` instead.
    #[inline]
    fn divides(&self, other: &BigInt) -> bool {
        return self.is_multiple_of(other);
    }

    /// Returns `true` if the number is a multiple of `other`.
    #[inline]
    fn is_multiple_of(&self, other: &BigInt) -> bool {
        self.data.is_multiple_of(&other.data)
    }

    /// Returns `true` if the number is divisible by `2`.
    #[inline]
    fn is_even(&self) -> bool {
        self.data.is_even()
    }

    /// Returns `true` if the number is not divisible by `2`.
    #[inline]
    fn is_odd(&self) -> bool {
        self.data.is_odd()
    }
}

impl ToPrimitive for BigInt {
    #[inline]
    fn to_i64(&self) -> Option<i64> {
        match self.sign {
            Plus => self.data.to_i64(),
            NoSign => Some(0),
            Minus => {
                self.data.to_u64().and_then(|n| {
                    let m: u64 = 1 << 63;
                    if n < m {
                        Some(-(n as i64))
                    } else if n == m {
                        Some(i64::MIN)
                    } else {
                        None
                    }
                })
            }
        }
    }

    #[inline]
    fn to_u64(&self) -> Option<u64> {
        match self.sign {
            Plus => self.data.to_u64(),
            NoSign => Some(0),
            Minus => None,
        }
    }

    #[inline]
    fn to_f32(&self) -> Option<f32> {
        self.data.to_f32().map(|n| {
            if self.sign == Minus {
                -n
            } else {
                n
            }
        })
    }

    #[inline]
    fn to_f64(&self) -> Option<f64> {
        self.data.to_f64().map(|n| {
            if self.sign == Minus {
                -n
            } else {
                n
            }
        })
    }
}

impl FromPrimitive for BigInt {
    #[inline]
    fn from_i64(n: i64) -> Option<BigInt> {
        Some(BigInt::from(n))
    }

    #[inline]
    fn from_u64(n: u64) -> Option<BigInt> {
        Some(BigInt::from(n))
    }

    #[inline]
    fn from_f64(n: f64) -> Option<BigInt> {
        if n >= 0.0 {
            BigUint::from_f64(n).map(|x| BigInt::from_biguint(Plus, x))
        } else {
            BigUint::from_f64(-n).map(|x| BigInt::from_biguint(Minus, x))
        }
    }
}

impl From<i64> for BigInt {
    #[inline]
    fn from(n: i64) -> Self {
        if n >= 0 {
            BigInt::from(n as u64)
        } else {
            let u = u64::MAX - (n as u64) + 1;
            BigInt {
                sign: Minus,
                data: BigUint::from(u),
            }
        }
    }
}

macro_rules! impl_bigint_from_int {
    ($T:ty) => {
        impl From<$T> for BigInt {
            #[inline]
            fn from(n: $T) -> Self {
                BigInt::from(n as i64)
            }
        }
    }
}

impl_bigint_from_int!(i8);
impl_bigint_from_int!(i16);
impl_bigint_from_int!(i32);
impl_bigint_from_int!(isize);

impl From<u64> for BigInt {
    #[inline]
    fn from(n: u64) -> Self {
        if n > 0 {
            BigInt {
                sign: Plus,
                data: BigUint::from(n),
            }
        } else {
            BigInt::zero()
        }
    }
}

macro_rules! impl_bigint_from_uint {
    ($T:ty) => {
        impl From<$T> for BigInt {
            #[inline]
            fn from(n: $T) -> Self {
                BigInt::from(n as u64)
            }
        }
    }
}

impl_bigint_from_uint!(u8);
impl_bigint_from_uint!(u16);
impl_bigint_from_uint!(u32);
impl_bigint_from_uint!(usize);

impl From<BigUint> for BigInt {
    #[inline]
    fn from(n: BigUint) -> Self {
        if n.is_zero() {
            BigInt::zero()
        } else {
            BigInt {
                sign: Plus,
                data: n,
            }
        }
    }
}

#[cfg(feature = "serde")]
impl serde::Serialize for BigInt {
    fn serialize<S>(&self, serializer: &mut S) -> Result<(), S::Error>
        where S: serde::Serializer
    {
        (self.sign, &self.data).serialize(serializer)
    }
}

#[cfg(feature = "serde")]
impl serde::Deserialize for BigInt {
    fn deserialize<D>(deserializer: &mut D) -> Result<Self, D::Error>
        where D: serde::Deserializer
    {
        let (sign, data) = try!(serde::Deserialize::deserialize(deserializer));
        Ok(BigInt {
            sign: sign,
            data: data,
        })
    }
}

/// A generic trait for converting a value to a `BigInt`.
pub trait ToBigInt {
    /// Converts the value of `self` to a `BigInt`.
    fn to_bigint(&self) -> Option<BigInt>;
}

impl ToBigInt for BigInt {
    #[inline]
    fn to_bigint(&self) -> Option<BigInt> {
        Some(self.clone())
    }
}

impl ToBigInt for BigUint {
    #[inline]
    fn to_bigint(&self) -> Option<BigInt> {
        if self.is_zero() {
            Some(Zero::zero())
        } else {
            Some(BigInt {
                sign: Plus,
                data: self.clone(),
            })
        }
    }
}

impl biguint::ToBigUint for BigInt {
    #[inline]
    fn to_biguint(&self) -> Option<BigUint> {
        match self.sign() {
            Plus    => Some(self.data.clone()),
            NoSign  => Some(Zero::zero()),
            Minus   => None,
        }
    }
}

macro_rules! impl_to_bigint {
    ($T:ty, $from_ty:path) => {
        impl ToBigInt for $T {
            #[inline]
            fn to_bigint(&self) -> Option<BigInt> {
                $from_ty(*self)
            }
        }
    }
}

impl_to_bigint!(isize, FromPrimitive::from_isize);
impl_to_bigint!(i8, FromPrimitive::from_i8);
impl_to_bigint!(i16, FromPrimitive::from_i16);
impl_to_bigint!(i32, FromPrimitive::from_i32);
impl_to_bigint!(i64, FromPrimitive::from_i64);
impl_to_bigint!(usize, FromPrimitive::from_usize);
impl_to_bigint!(u8, FromPrimitive::from_u8);
impl_to_bigint!(u16, FromPrimitive::from_u16);
impl_to_bigint!(u32, FromPrimitive::from_u32);
impl_to_bigint!(u64, FromPrimitive::from_u64);
impl_to_bigint!(f32, FromPrimitive::from_f32);
impl_to_bigint!(f64, FromPrimitive::from_f64);

pub trait RandBigInt {
    /// Generate a random `BigUint` of the given bit size.
    fn gen_biguint(&mut self, bit_size: usize) -> BigUint;

    /// Generate a random BigInt of the given bit size.
    fn gen_bigint(&mut self, bit_size: usize) -> BigInt;

    /// Generate a random `BigUint` less than the given bound. Fails
    /// when the bound is zero.
    fn gen_biguint_below(&mut self, bound: &BigUint) -> BigUint;

    /// Generate a random `BigUint` within the given range. The lower
    /// bound is inclusive; the upper bound is exclusive. Fails when
    /// the upper bound is not greater than the lower bound.
    fn gen_biguint_range(&mut self, lbound: &BigUint, ubound: &BigUint) -> BigUint;

    /// Generate a random `BigInt` within the given range. The lower
    /// bound is inclusive; the upper bound is exclusive. Fails when
    /// the upper bound is not greater than the lower bound.
    fn gen_bigint_range(&mut self, lbound: &BigInt, ubound: &BigInt) -> BigInt;
}

#[cfg(any(feature = "rand", test))]
impl<R: Rng> RandBigInt for R {
    fn gen_biguint(&mut self, bit_size: usize) -> BigUint {
        let (digits, rem) = bit_size.div_rem(&big_digit::BITS);
        let mut data = Vec::with_capacity(digits + 1);
        for _ in 0..digits {
            data.push(self.gen());
        }
        if rem > 0 {
            let final_digit: BigDigit = self.gen();
            data.push(final_digit >> (big_digit::BITS - rem));
        }
        BigUint::new(data)
    }

    fn gen_bigint(&mut self, bit_size: usize) -> BigInt {
        // Generate a random BigUint...
        let biguint = self.gen_biguint(bit_size);
        // ...and then randomly assign it a Sign...
        let sign = if biguint.is_zero() {
            // ...except that if the BigUint is zero, we need to try
            // again with probability 0.5. This is because otherwise,
            // the probability of generating a zero BigInt would be
            // double that of any other number.
            if self.gen() {
                return self.gen_bigint(bit_size);
            } else {
                NoSign
            }
        } else if self.gen() {
            Plus
        } else {
            Minus
        };
        BigInt::from_biguint(sign, biguint)
    }

    fn gen_biguint_below(&mut self, bound: &BigUint) -> BigUint {
        assert!(!bound.is_zero());
        let bits = bound.bits();
        loop {
            let n = self.gen_biguint(bits);
            if n < *bound {
                return n;
            }
        }
    }

    fn gen_biguint_range(&mut self, lbound: &BigUint, ubound: &BigUint) -> BigUint {
        assert!(*lbound < *ubound);
        return lbound + self.gen_biguint_below(&(ubound - lbound));
    }

    fn gen_bigint_range(&mut self, lbound: &BigInt, ubound: &BigInt) -> BigInt {
        assert!(*lbound < *ubound);
        let delta = (ubound - lbound).to_biguint().unwrap();
        return lbound + self.gen_biguint_below(&delta).to_bigint().unwrap();
    }
}

impl BigInt {
    /// Creates and initializes a BigInt.
    ///
    /// The digits are in little-endian base 2<sup>32</sup>.
    #[inline]
    pub fn new(sign: Sign, digits: Vec<BigDigit>) -> BigInt {
        BigInt::from_biguint(sign, BigUint::new(digits))
    }

    /// Creates and initializes a `BigInt`.
    ///
    /// The digits are in little-endian base 2<sup>32</sup>.
    #[inline]
    pub fn from_biguint(mut sign: Sign, mut data: BigUint) -> BigInt {
        if sign == NoSign {
            data.assign_from_slice(&[]);
        } else if data.is_zero() {
            sign = NoSign;
        }

        BigInt {
            sign: sign,
            data: data,
        }
    }

    /// Creates and initializes a `BigInt`.
    #[inline]
    pub fn from_slice(sign: Sign, slice: &[BigDigit]) -> BigInt {
        BigInt::from_biguint(sign, BigUint::from_slice(slice))
    }

    /// Reinitializes a `BigInt`.
    #[inline]
    pub fn assign_from_slice(&mut self, sign: Sign, slice: &[BigDigit]) {
        if sign == NoSign {
            self.data.assign_from_slice(&[]);
            self.sign = NoSign;
        } else {
            self.data.assign_from_slice(slice);
            self.sign = match self.data.is_zero() {
                true => NoSign,
                false => sign,
            }
        }
    }

    /// Creates and initializes a `BigInt`.
    ///
    /// The bytes are in big-endian byte order.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{BigInt, Sign};
    ///
    /// assert_eq!(BigInt::from_bytes_be(Sign::Plus, b"A"),
    ///            BigInt::parse_bytes(b"65", 10).unwrap());
    /// assert_eq!(BigInt::from_bytes_be(Sign::Plus, b"AA"),
    ///            BigInt::parse_bytes(b"16705", 10).unwrap());
    /// assert_eq!(BigInt::from_bytes_be(Sign::Plus, b"AB"),
    ///            BigInt::parse_bytes(b"16706", 10).unwrap());
    /// assert_eq!(BigInt::from_bytes_be(Sign::Plus, b"Hello world!"),
    ///            BigInt::parse_bytes(b"22405534230753963835153736737", 10).unwrap());
    /// ```
    #[inline]
    pub fn from_bytes_be(sign: Sign, bytes: &[u8]) -> BigInt {
        BigInt::from_biguint(sign, BigUint::from_bytes_be(bytes))
    }

    /// Creates and initializes a `BigInt`.
    ///
    /// The bytes are in little-endian byte order.
    #[inline]
    pub fn from_bytes_le(sign: Sign, bytes: &[u8]) -> BigInt {
        BigInt::from_biguint(sign, BigUint::from_bytes_le(bytes))
    }

    /// Creates and initializes a `BigInt` from an array of bytes in
    /// two's complement binary representation.
    ///
    /// The digits are in big-endian base 2<sup>8</sup>.
    #[inline]
    pub fn from_signed_bytes_be(digits: &[u8]) -> BigInt {
        let sign = match digits.first() {
            Some(v) if *v > 0x7f => Sign::Minus,
            Some(_) => Sign::Plus,
            None => return BigInt::zero(),
        };

        if sign == Sign::Minus {
            // two's-complement the content to retrieve the magnitude
            let mut digits = Vec::from(digits);
            twos_complement_be(&mut digits);
            BigInt::from_biguint(sign, BigUint::from_bytes_be(&*digits))
        } else {
            BigInt::from_biguint(sign, BigUint::from_bytes_be(digits))
        }
    }

    /// Creates and initializes a `BigInt` from an array of bytes in two's complement.
    ///
    /// The digits are in little-endian base 2<sup>8</sup>.
    #[inline]
    pub fn from_signed_bytes_le(digits: &[u8]) -> BigInt {
        let sign = match digits.last() {
            Some(v) if *v > 0x7f => Sign::Minus,
            Some(_) => Sign::Plus,
            None => return BigInt::zero(),
        };

        if sign == Sign::Minus {
            // two's-complement the content to retrieve the magnitude
            let mut digits = Vec::from(digits);
            twos_complement_le(&mut digits);
            BigInt::from_biguint(sign, BigUint::from_bytes_le(&*digits))
        } else {
            BigInt::from_biguint(sign, BigUint::from_bytes_le(digits))
        }
    }

    /// Creates and initializes a `BigInt`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{BigInt, ToBigInt};
    ///
    /// assert_eq!(BigInt::parse_bytes(b"1234", 10), ToBigInt::to_bigint(&1234));
    /// assert_eq!(BigInt::parse_bytes(b"ABCD", 16), ToBigInt::to_bigint(&0xABCD));
    /// assert_eq!(BigInt::parse_bytes(b"G", 16), None);
    /// ```
    #[inline]
    pub fn parse_bytes(buf: &[u8], radix: u32) -> Option<BigInt> {
        str::from_utf8(buf).ok().and_then(|s| BigInt::from_str_radix(s, radix).ok())
    }

    /// Creates and initializes a `BigInt`. Each u8 of the input slice is
    /// interpreted as one digit of the number
    /// and must therefore be less than `radix`.
    ///
    /// The bytes are in big-endian byte order.
    /// `radix` must be in the range `2...256`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{BigInt, Sign};
    ///
    /// let inbase190 = vec![15, 33, 125, 12, 14];
    /// let a = BigInt::from_radix_be(Sign::Minus, &inbase190, 190).unwrap();
    /// assert_eq!(a.to_radix_be(190), (Sign:: Minus, inbase190));
    /// ```
    pub fn from_radix_be(sign: Sign, buf: &[u8], radix: u32) -> Option<BigInt> {
        BigUint::from_radix_be(buf, radix).map(|u| BigInt::from_biguint(sign, u))
    }

    /// Creates and initializes a `BigInt`. Each u8 of the input slice is
    /// interpreted as one digit of the number
    /// and must therefore be less than `radix`.
    ///
    /// The bytes are in little-endian byte order.
    /// `radix` must be in the range `2...256`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{BigInt, Sign};
    ///
    /// let inbase190 = vec![14, 12, 125, 33, 15];
    /// let a = BigInt::from_radix_be(Sign::Minus, &inbase190, 190).unwrap();
    /// assert_eq!(a.to_radix_be(190), (Sign::Minus, inbase190));
    /// ```
    pub fn from_radix_le(sign: Sign, buf: &[u8], radix: u32) -> Option<BigInt> {
        BigUint::from_radix_le(buf, radix).map(|u| BigInt::from_biguint(sign, u))
    }

    /// Returns the sign and the byte representation of the `BigInt` in big-endian byte order.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{ToBigInt, Sign};
    ///
    /// let i = -1125.to_bigint().unwrap();
    /// assert_eq!(i.to_bytes_be(), (Sign::Minus, vec![4, 101]));
    /// ```
    #[inline]
    pub fn to_bytes_be(&self) -> (Sign, Vec<u8>) {
        (self.sign, self.data.to_bytes_be())
    }

    /// Returns the sign and the byte representation of the `BigInt` in little-endian byte order.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{ToBigInt, Sign};
    ///
    /// let i = -1125.to_bigint().unwrap();
    /// assert_eq!(i.to_bytes_le(), (Sign::Minus, vec![101, 4]));
    /// ```
    #[inline]
    pub fn to_bytes_le(&self) -> (Sign, Vec<u8>) {
        (self.sign, self.data.to_bytes_le())
    }

    /// Returns the two's complement byte representation of the `BigInt` in big-endian byte order.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::ToBigInt;
    ///
    /// let i = -1125.to_bigint().unwrap();
    /// assert_eq!(i.to_signed_bytes_be(), vec![251, 155]);
    /// ```
    #[inline]
    pub fn to_signed_bytes_be(&self) -> Vec<u8> {
        let mut bytes = self.data.to_bytes_be();
        let first_byte = bytes.first().map(|v| *v).unwrap_or(0);
        if first_byte > 0x7f && !(first_byte == 0x80 && bytes.iter().skip(1).all(Zero::is_zero)) {
            // msb used by magnitude, extend by 1 byte
            bytes.insert(0, 0);
        }
        if self.sign == Sign::Minus {
            twos_complement_be(&mut bytes);
        }
        bytes
    }

    /// Returns the two's complement byte representation of the `BigInt` in little-endian byte order.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::ToBigInt;
    ///
    /// let i = -1125.to_bigint().unwrap();
    /// assert_eq!(i.to_signed_bytes_le(), vec![155, 251]);
    /// ```
    #[inline]
    pub fn to_signed_bytes_le(&self) -> Vec<u8> {
        let mut bytes = self.data.to_bytes_le();
        let last_byte = bytes.last().map(|v| *v).unwrap_or(0);
        if last_byte > 0x7f && !(last_byte == 0x80 && bytes.iter().rev().skip(1).all(Zero::is_zero)) {
            // msb used by magnitude, extend by 1 byte
            bytes.push(0);
        }
        if self.sign == Sign::Minus {
            twos_complement_le(&mut bytes);
        }
        bytes
    }

    /// Returns the integer formatted as a string in the given radix.
    /// `radix` must be in the range `2...36`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::BigInt;
    ///
    /// let i = BigInt::parse_bytes(b"ff", 16).unwrap();
    /// assert_eq!(i.to_str_radix(16), "ff");
    /// ```
    #[inline]
    pub fn to_str_radix(&self, radix: u32) -> String {
        let mut v = to_str_radix_reversed(&self.data, radix);

        if self.is_negative() {
            v.push(b'-');
        }

        v.reverse();
        unsafe { String::from_utf8_unchecked(v) }
    }

    /// Returns the integer in the requested base in big-endian digit order.
    /// The output is not given in a human readable alphabet but as a zero
    /// based u8 number.
    /// `radix` must be in the range `2...256`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{BigInt, Sign};
    ///
    /// assert_eq!(BigInt::from(-0xFFFFi64).to_radix_be(159),
    ///            (Sign::Minus, vec![2, 94, 27]));
    /// // 0xFFFF = 65535 = 2*(159^2) + 94*159 + 27
    /// ```
    #[inline]
    pub fn to_radix_be(&self, radix: u32) -> (Sign, Vec<u8>) {
        (self.sign, self.data.to_radix_be(radix))
    }

    /// Returns the integer in the requested base in little-endian digit order.
    /// The output is not given in a human readable alphabet but as a zero
    /// based u8 number.
    /// `radix` must be in the range `2...256`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{BigInt, Sign};
    ///
    /// assert_eq!(BigInt::from(-0xFFFFi64).to_radix_le(159),
    ///            (Sign::Minus, vec![27, 94, 2]));
    /// // 0xFFFF = 65535 = 27 + 94*159 + 2*(159^2)
    /// ```
    #[inline]
    pub fn to_radix_le(&self, radix: u32) -> (Sign, Vec<u8>) {
        (self.sign, self.data.to_radix_le(radix))
    }

    /// Returns the sign of the `BigInt` as a `Sign`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{ToBigInt, Sign};
    ///
    /// assert_eq!(ToBigInt::to_bigint(&1234).unwrap().sign(), Sign::Plus);
    /// assert_eq!(ToBigInt::to_bigint(&-4321).unwrap().sign(), Sign::Minus);
    /// assert_eq!(ToBigInt::to_bigint(&0).unwrap().sign(), Sign::NoSign);
    /// ```
    #[inline]
    pub fn sign(&self) -> Sign {
        self.sign
    }

    /// Determines the fewest bits necessary to express the `BigInt`,
    /// not including the sign.
    #[inline]
    pub fn bits(&self) -> usize {
        self.data.bits()
    }

    /// Converts this `BigInt` into a `BigUint`, if it's not negative.
    #[inline]
    pub fn to_biguint(&self) -> Option<BigUint> {
        match self.sign {
            Plus => Some(self.data.clone()),
            NoSign => Some(Zero::zero()),
            Minus => None,
        }
    }

    #[inline]
    pub fn checked_add(&self, v: &BigInt) -> Option<BigInt> {
        return Some(self.add(v));
    }

    #[inline]
    pub fn checked_sub(&self, v: &BigInt) -> Option<BigInt> {
        return Some(self.sub(v));
    }

    #[inline]
    pub fn checked_mul(&self, v: &BigInt) -> Option<BigInt> {
        return Some(self.mul(v));
    }

    #[inline]
    pub fn checked_div(&self, v: &BigInt) -> Option<BigInt> {
        if v.is_zero() {
            return None;
        }
        return Some(self.div(v));
    }

    /// Returns `(self ^ exponent) mod modulus`
    ///
    /// Note that this rounds like `mod_floor`, not like the `%` operator,
    /// which makes a difference when given a negative `self` or `modulus`.
    /// The result will be in the interval `[0, modulus)` for `modulus > 0`,
    /// or in the interval `(modulus, 0]` for `modulus < 0`
    ///
    /// Panics if the exponent is negative or the modulus is zero.
    pub fn modpow(&self, exponent: &Self, modulus: &Self) -> Self {
        assert!(!exponent.is_negative(), "negative exponentiation is not supported!");
        assert!(!modulus.is_zero(), "divide by zero!");

        let result = self.data.modpow(&exponent.data, &modulus.data);
        if result.is_zero() {
            return BigInt::zero();
        }

        // The sign of the result follows the modulus, like `mod_floor`.
        let (sign, mag) = match (self.is_negative(), modulus.is_negative()) {
            (false, false) => (Plus, result),
            (true, false) => (Plus, &modulus.data - result),
            (false, true) => (Minus, &modulus.data - result),
            (true, true) => (Minus, result),
        };
        BigInt::from_biguint(sign, mag)
    }
}

/// Perform in-place two's complement of the given binary representation,
/// in little-endian byte order.
#[inline]
fn twos_complement_le(digits: &mut [u8]) {
    twos_complement(digits)
}

/// Perform in-place two's complement of the given binary representation
/// in big-endian byte order.
#[inline]
fn twos_complement_be(digits: &mut [u8]) {
    twos_complement(digits.iter_mut().rev())
}

/// Perform in-place two's complement of the given digit iterator
/// starting from the least significant byte.
#[inline]
fn twos_complement<'a, I>(digits: I)
    where I: IntoIterator<Item = &'a mut u8>
{
    let mut carry = true;
    for d in digits {
        *d = d.not();
        if carry {
            *d = d.wrapping_add(1);
            carry = d.is_zero();
        }
    }
}
