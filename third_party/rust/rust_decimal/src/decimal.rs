use crate::constants::{
    MAX_I128_REPR, MAX_PRECISION, POWERS_10, SCALE_MASK, SCALE_SHIFT, SIGN_MASK, SIGN_SHIFT, U32_MASK, U8_MASK,
    UNSIGN_MASK,
};
use crate::ops;
use crate::Error;

use core::{
    cmp::{Ordering::Equal, *},
    fmt,
    hash::{Hash, Hasher},
    iter::Sum,
    ops::{Add, AddAssign, Div, DivAssign, Mul, MulAssign, Neg, Rem, RemAssign, Sub, SubAssign},
    str::FromStr,
};
#[cfg(feature = "diesel")]
use diesel::sql_types::Numeric;
#[allow(unused_imports)] // It's not actually dead code below, but the compiler thinks it is.
#[cfg(not(feature = "std"))]
use num_traits::float::FloatCore;
use num_traits::{
    CheckedAdd, CheckedDiv, CheckedMul, CheckedRem, CheckedSub, FromPrimitive, Num, One, Signed, ToPrimitive, Zero,
};

/// The smallest value that can be represented by this decimal type.
const MIN: Decimal = Decimal {
    flags: 2_147_483_648,
    lo: 4_294_967_295,
    mid: 4_294_967_295,
    hi: 4_294_967_295,
};

/// The largest value that can be represented by this decimal type.
const MAX: Decimal = Decimal {
    flags: 0,
    lo: 4_294_967_295,
    mid: 4_294_967_295,
    hi: 4_294_967_295,
};

/// A constant representing 0.
const ZERO: Decimal = Decimal {
    flags: 0,
    lo: 0,
    mid: 0,
    hi: 0,
};

/// A constant representing 1.
const ONE: Decimal = Decimal {
    flags: 0,
    lo: 1,
    mid: 0,
    hi: 0,
};

/// `UnpackedDecimal` contains unpacked representation of `Decimal` where each component
/// of decimal-format stored in it's own field
#[derive(Clone, Copy, Debug)]
pub struct UnpackedDecimal {
    pub negative: bool,
    pub scale: u32,
    pub hi: u32,
    pub mid: u32,
    pub lo: u32,
}

/// `Decimal` represents a 128 bit representation of a fixed-precision decimal number.
/// The finite set of values of type `Decimal` are of the form m / 10<sup>e</sup>,
/// where m is an integer such that -2<sup>96</sup> < m < 2<sup>96</sup>, and e is an integer
/// between 0 and 28 inclusive.
#[derive(Clone, Copy)]
#[cfg_attr(feature = "diesel", derive(FromSqlRow, AsExpression), sql_type = "Numeric")]
#[cfg_attr(feature = "c-repr", repr(C))]
pub struct Decimal {
    // Bits 0-15: unused
    // Bits 16-23: Contains "e", a value between 0-28 that indicates the scale
    // Bits 24-30: unused
    // Bit 31: the sign of the Decimal value, 0 meaning positive and 1 meaning negative.
    flags: u32,
    // The lo, mid, hi, and flags fields contain the representation of the
    // Decimal value as a 96-bit integer.
    hi: u32,
    lo: u32,
    mid: u32,
}

/// `RoundingStrategy` represents the different rounding strategies that can be used by
/// `round_dp_with_strategy`.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum RoundingStrategy {
    /// When a number is halfway between two others, it is rounded toward the nearest even number.
    /// Also known as "Bankers Rounding".
    /// e.g.
    /// 6.5 -> 6, 7.5 -> 8
    MidpointNearestEven,
    /// When a number is halfway between two others, it is rounded toward the nearest number that
    /// is away from zero. e.g. 6.4 -> 6, 6.5 -> 7, -6.5 -> -7
    MidpointAwayFromZero,
    /// When a number is halfway between two others, it is rounded toward the nearest number that
    /// is toward zero. e.g. 6.4 -> 6, 6.5 -> 7, -6.5 -> -6
    MidpointTowardZero,
    /// The number is always rounded toward zero. e.g. -6.8 -> -6, 6.8 -> 6
    ToZero,
    /// The number is always rounded away from zero. e.g. -6.8 -> -7, 6.8 -> 7
    AwayFromZero,
    /// The number is always rounded towards negative infinity. e.g. 6.8 -> 6, -6.8 -> -7
    ToNegativeInfinity,
    /// The number is always rounded towards positive infinity. e.g. 6.8 -> 7, -6.8 -> -6
    ToPositiveInfinity,

    /// When a number is halfway between two others, it is rounded toward the nearest even number.
    /// e.g.
    /// 6.5 -> 6, 7.5 -> 8
    #[deprecated(since = "1.11.0", note = "Please use RoundingStrategy::MidpointNearestEven instead")]
    BankersRounding,
    /// Rounds up if the value >= 5, otherwise rounds down, e.g. 6.5 -> 7
    #[deprecated(since = "1.11.0", note = "Please use RoundingStrategy::MidpointAwayFromZero instead")]
    RoundHalfUp,
    /// Rounds down if the value =< 5, otherwise rounds up, e.g. 6.5 -> 6, 6.51 -> 7 1.4999999 -> 1
    #[deprecated(since = "1.11.0", note = "Please use RoundingStrategy::MidpointTowardZero instead")]
    RoundHalfDown,
    /// Always round down.
    #[deprecated(since = "1.11.0", note = "Please use RoundingStrategy::ToZero instead")]
    RoundDown,
    /// Always round up.
    #[deprecated(since = "1.11.0", note = "Please use RoundingStrategy::AwayFromZero instead")]
    RoundUp,
}

#[allow(dead_code)]
impl Decimal {
    /// The smallest value that can be represented by this decimal type.
    pub const MIN: Decimal = MIN;
    /// The largest value that can be represented by this decimal type.
    pub const MAX: Decimal = MAX;
    /// A constant representing 0.
    pub const ZERO: Decimal = ZERO;
    /// A constant representing 1.
    pub const ONE: Decimal = ONE;

    /// Returns a `Decimal` with a 64 bit `m` representation and corresponding `e` scale.
    ///
    /// # Arguments
    ///
    /// * `num` - An i64 that represents the `m` portion of the decimal number
    /// * `scale` - A u32 representing the `e` portion of the decimal number.
    ///
    /// # Panics
    ///
    /// This function panics if `scale` is > 28.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let pi = Decimal::new(3141, 3);
    /// assert_eq!(pi.to_string(), "3.141");
    /// ```
    #[must_use]
    pub fn new(num: i64, scale: u32) -> Decimal {
        if scale > MAX_PRECISION {
            panic!(
                "Scale exceeds the maximum precision allowed: {} > {}",
                scale, MAX_PRECISION
            );
        }
        let flags: u32 = scale << SCALE_SHIFT;
        if num < 0 {
            let pos_num = num.wrapping_neg() as u64;
            return Decimal {
                flags: flags | SIGN_MASK,
                hi: 0,
                lo: (pos_num & U32_MASK) as u32,
                mid: ((pos_num >> 32) & U32_MASK) as u32,
            };
        }
        Decimal {
            flags,
            hi: 0,
            lo: (num as u64 & U32_MASK) as u32,
            mid: ((num as u64 >> 32) & U32_MASK) as u32,
        }
    }

    /// Creates a `Decimal` using a 128 bit signed `m` representation and corresponding `e` scale.
    ///
    /// # Arguments
    ///
    /// * `num` - An i128 that represents the `m` portion of the decimal number
    /// * `scale` - A u32 representing the `e` portion of the decimal number.
    ///
    /// # Panics
    ///
    /// This function panics if `scale` is > 28 or if `num` exceeds the maximum supported 96 bits.
    ///
    /// # Example
    ///
    /// ```rust
    /// use rust_decimal::Decimal;
    ///
    /// let pi = Decimal::from_i128_with_scale(3141i128, 3);
    /// assert_eq!(pi.to_string(), "3.141");
    /// ```
    #[must_use]
    pub fn from_i128_with_scale(num: i128, scale: u32) -> Decimal {
        match Self::try_from_i128_with_scale(num, scale) {
            Ok(d) => d,
            Err(e) => panic!("{}", e),
        }
    }

    /// Checked version of `from_i128_with_scale`. Will return `Err` instead
    /// of panicking at run-time.
    ///
    /// # Example
    ///
    /// ```rust
    /// use rust_decimal::Decimal;
    ///
    /// let max = Decimal::try_from_i128_with_scale(i128::MAX, u32::MAX);
    /// assert!(max.is_err());
    /// ```
    pub fn try_from_i128_with_scale(num: i128, scale: u32) -> crate::Result<Decimal> {
        if scale > MAX_PRECISION {
            return Err(Error::ScaleExceedsMaximumPrecision(scale));
        }
        let mut neg = false;
        let mut wrapped = num;
        if num > MAX_I128_REPR {
            return Err(Error::ExceedsMaximumPossibleValue);
        } else if num < -MAX_I128_REPR {
            return Err(Error::LessThanMinimumPossibleValue);
        } else if num < 0 {
            neg = true;
            wrapped = -num;
        }
        let flags: u32 = flags(neg, scale);
        Ok(Decimal {
            flags,
            lo: (wrapped as u64 & U32_MASK) as u32,
            mid: ((wrapped as u64 >> 32) & U32_MASK) as u32,
            hi: ((wrapped as u128 >> 64) as u64 & U32_MASK) as u32,
        })
    }

    /// Returns a `Decimal` using the instances constituent parts.
    ///
    /// # Arguments
    ///
    /// * `lo` - The low 32 bits of a 96-bit integer.
    /// * `mid` - The middle 32 bits of a 96-bit integer.
    /// * `hi` - The high 32 bits of a 96-bit integer.
    /// * `negative` - `true` to indicate a negative number.
    /// * `scale` - A power of 10 ranging from 0 to 28.
    ///
    /// # Caution: Undefined behavior
    ///
    /// While a scale greater than 28 can be passed in, it will be automatically capped by this
    /// function at the maximum precision. The library opts towards this functionality as opposed
    /// to a panic to ensure that the function can be treated as constant. This may lead to
    /// undefined behavior in downstream applications and should be treated with caution.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let pi = Decimal::from_parts(1102470952, 185874565, 1703060790, false, 28);
    /// assert_eq!(pi.to_string(), "3.1415926535897932384626433832");
    /// ```
    #[must_use]
    pub const fn from_parts(lo: u32, mid: u32, hi: u32, negative: bool, scale: u32) -> Decimal {
        Decimal {
            lo,
            mid,
            hi,
            flags: flags(
                if lo == 0 && mid == 0 && hi == 0 {
                    false
                } else {
                    negative
                },
                scale % (MAX_PRECISION + 1),
            ),
        }
    }

    #[must_use]
    pub(crate) const fn from_parts_raw(lo: u32, mid: u32, hi: u32, flags: u32) -> Decimal {
        if lo == 0 && mid == 0 && hi == 0 {
            Decimal {
                lo,
                mid,
                hi,
                flags: flags & SCALE_MASK,
            }
        } else {
            Decimal { flags, hi, lo, mid }
        }
    }

    /// Returns a `Result` which if successful contains the `Decimal` constitution of
    /// the scientific notation provided by `value`.
    ///
    /// # Arguments
    ///
    /// * `value` - The scientific notation of the `Decimal`.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let value = Decimal::from_scientific("9.7e-7").unwrap();
    /// assert_eq!(value.to_string(), "0.00000097");
    /// ```
    pub fn from_scientific(value: &str) -> Result<Decimal, Error> {
        const ERROR_MESSAGE: &str = "Failed to parse";

        let mut split = value.splitn(2, |c| c == 'e' || c == 'E');

        let base = split.next().ok_or_else(|| Error::from(ERROR_MESSAGE))?;
        let exp = split.next().ok_or_else(|| Error::from(ERROR_MESSAGE))?;

        let mut ret = Decimal::from_str(base)?;
        let current_scale = ret.scale();

        if let Some(stripped) = exp.strip_prefix('-') {
            let exp: u32 = stripped.parse().map_err(|_| Error::from(ERROR_MESSAGE))?;
            ret.set_scale(current_scale + exp)?;
        } else {
            let exp: u32 = exp.parse().map_err(|_| Error::from(ERROR_MESSAGE))?;
            if exp <= current_scale {
                ret.set_scale(current_scale - exp)?;
            } else if exp > 0 {
                use crate::constants::BIG_POWERS_10;

                // This is a case whereby the mantissa needs to be larger to be correctly
                // represented within the decimal type. A good example is 1.2E10. At this point,
                // we've parsed 1.2 as the base and 10 as the exponent. To represent this within a
                // Decimal type we effectively store the mantissa as 12,000,000,000 and scale as
                // zero.
                if exp > MAX_PRECISION {
                    return Err(Error::ScaleExceedsMaximumPrecision(exp));
                }
                let mut exp = exp as usize;
                // Max two iterations. If exp is 1 then it needs to index position 0 of the array.
                while exp > 0 {
                    let pow;
                    if exp >= BIG_POWERS_10.len() {
                        pow = BIG_POWERS_10[BIG_POWERS_10.len() - 1];
                        exp -= BIG_POWERS_10.len();
                    } else {
                        pow = BIG_POWERS_10[exp - 1];
                        exp = 0;
                    }

                    let pow = Decimal {
                        flags: 0,
                        lo: pow as u32,
                        mid: (pow >> 32) as u32,
                        hi: 0,
                    };
                    match ret.checked_mul(pow) {
                        Some(r) => ret = r,
                        None => return Err(Error::ExceedsMaximumPossibleValue),
                    };
                }
                ret.normalize_assign();
            }
        }
        Ok(ret)
    }

    /// Returns the scale of the decimal number, otherwise known as `e`.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let num = Decimal::new(1234, 3);
    /// assert_eq!(num.scale(), 3u32);
    /// ```
    #[inline]
    #[must_use]
    pub const fn scale(&self) -> u32 {
        ((self.flags & SCALE_MASK) >> SCALE_SHIFT) as u32
    }

    /// Returns the mantissa of the decimal number.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::prelude::*;
    ///
    /// let num = Decimal::from_str("-1.2345678").unwrap();
    /// assert_eq!(num.mantissa(), -12345678i128);
    /// assert_eq!(num.scale(), 7);
    /// ```
    #[must_use]
    pub const fn mantissa(&self) -> i128 {
        let raw = (self.lo as i128) | ((self.mid as i128) << 32) | ((self.hi as i128) << 64);
        if self.is_sign_negative() {
            -raw
        } else {
            raw
        }
    }

    /// Returns true if this Decimal number is equivalent to zero.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::prelude::*;
    ///
    /// let num = Decimal::ZERO;
    /// assert!(num.is_zero());
    /// ```
    #[must_use]
    pub const fn is_zero(&self) -> bool {
        self.lo == 0 && self.mid == 0 && self.hi == 0
    }

    /// An optimized method for changing the sign of a decimal number.
    ///
    /// # Arguments
    ///
    /// * `positive`: true if the resulting decimal should be positive.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let mut one = Decimal::new(1, 0);
    /// one.set_sign(false);
    /// assert_eq!(one.to_string(), "-1");
    /// ```
    #[deprecated(since = "1.4.0", note = "please use `set_sign_positive` instead")]
    pub fn set_sign(&mut self, positive: bool) {
        self.set_sign_positive(positive);
    }

    /// An optimized method for changing the sign of a decimal number.
    ///
    /// # Arguments
    ///
    /// * `positive`: true if the resulting decimal should be positive.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let mut one = Decimal::new(1, 0);
    /// one.set_sign_positive(false);
    /// assert_eq!(one.to_string(), "-1");
    /// ```
    #[inline(always)]
    pub fn set_sign_positive(&mut self, positive: bool) {
        if positive {
            self.flags &= UNSIGN_MASK;
        } else {
            self.flags |= SIGN_MASK;
        }
    }

    /// An optimized method for changing the sign of a decimal number.
    ///
    /// # Arguments
    ///
    /// * `negative`: true if the resulting decimal should be negative.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let mut one = Decimal::new(1, 0);
    /// one.set_sign_negative(true);
    /// assert_eq!(one.to_string(), "-1");
    /// ```
    #[inline(always)]
    pub fn set_sign_negative(&mut self, negative: bool) {
        self.set_sign_positive(!negative);
    }

    /// An optimized method for changing the scale of a decimal number.
    ///
    /// # Arguments
    ///
    /// * `scale`: the new scale of the number
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let mut one = Decimal::new(1, 0);
    /// one.set_scale(5).unwrap();
    /// assert_eq!(one.to_string(), "0.00001");
    /// ```
    pub fn set_scale(&mut self, scale: u32) -> Result<(), Error> {
        if scale > MAX_PRECISION {
            return Err(Error::ScaleExceedsMaximumPrecision(scale));
        }
        self.flags = (scale << SCALE_SHIFT) | (self.flags & SIGN_MASK);
        Ok(())
    }

    /// Modifies the `Decimal` to the given scale, attempting to do so without changing the
    /// underlying number itself.
    ///
    /// Note that setting the scale to something less then the current `Decimal`s scale will
    /// cause the newly created `Decimal` to have some rounding.
    /// Scales greater than the maximum precision supported by `Decimal` will be automatically
    /// rounded to `Decimal::MAX_PRECISION`.
    /// Rounding leverages the half up strategy.
    ///
    /// # Arguments
    /// * `scale`: The scale to use for the new `Decimal` number.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let mut number = Decimal::new(1_123, 3);
    /// number.rescale(6);
    /// assert_eq!(number, Decimal::new(1_123_000, 6));
    /// let mut round = Decimal::new(145, 2);
    /// round.rescale(1);
    /// assert_eq!(round, Decimal::new(15, 1));
    /// ```
    pub fn rescale(&mut self, scale: u32) {
        let mut array = [self.lo, self.mid, self.hi];
        let mut value_scale = self.scale();
        ops::array::rescale_internal(&mut array, &mut value_scale, scale);
        self.lo = array[0];
        self.mid = array[1];
        self.hi = array[2];
        self.flags = flags(self.is_sign_negative(), value_scale);
    }

    /// Returns a serialized version of the decimal number.
    /// The resulting byte array will have the following representation:
    ///
    /// * Bytes 1-4: flags
    /// * Bytes 5-8: lo portion of `m`
    /// * Bytes 9-12: mid portion of `m`
    /// * Bytes 13-16: high portion of `m`
    #[must_use]
    pub const fn serialize(&self) -> [u8; 16] {
        [
            (self.flags & U8_MASK) as u8,
            ((self.flags >> 8) & U8_MASK) as u8,
            ((self.flags >> 16) & U8_MASK) as u8,
            ((self.flags >> 24) & U8_MASK) as u8,
            (self.lo & U8_MASK) as u8,
            ((self.lo >> 8) & U8_MASK) as u8,
            ((self.lo >> 16) & U8_MASK) as u8,
            ((self.lo >> 24) & U8_MASK) as u8,
            (self.mid & U8_MASK) as u8,
            ((self.mid >> 8) & U8_MASK) as u8,
            ((self.mid >> 16) & U8_MASK) as u8,
            ((self.mid >> 24) & U8_MASK) as u8,
            (self.hi & U8_MASK) as u8,
            ((self.hi >> 8) & U8_MASK) as u8,
            ((self.hi >> 16) & U8_MASK) as u8,
            ((self.hi >> 24) & U8_MASK) as u8,
        ]
    }

    /// Deserializes the given bytes into a decimal number.
    /// The deserialized byte representation must be 16 bytes and adhere to the following convention:
    ///
    /// * Bytes 1-4: flags
    /// * Bytes 5-8: lo portion of `m`
    /// * Bytes 9-12: mid portion of `m`
    /// * Bytes 13-16: high portion of `m`
    #[must_use]
    pub const fn deserialize(bytes: [u8; 16]) -> Decimal {
        Decimal {
            flags: (bytes[0] as u32) | (bytes[1] as u32) << 8 | (bytes[2] as u32) << 16 | (bytes[3] as u32) << 24,
            lo: (bytes[4] as u32) | (bytes[5] as u32) << 8 | (bytes[6] as u32) << 16 | (bytes[7] as u32) << 24,
            mid: (bytes[8] as u32) | (bytes[9] as u32) << 8 | (bytes[10] as u32) << 16 | (bytes[11] as u32) << 24,
            hi: (bytes[12] as u32) | (bytes[13] as u32) << 8 | (bytes[14] as u32) << 16 | (bytes[15] as u32) << 24,
        }
    }

    /// Returns `true` if the decimal is negative.
    #[deprecated(since = "0.6.3", note = "please use `is_sign_negative` instead")]
    #[must_use]
    pub fn is_negative(&self) -> bool {
        self.is_sign_negative()
    }

    /// Returns `true` if the decimal is positive.
    #[deprecated(since = "0.6.3", note = "please use `is_sign_positive` instead")]
    #[must_use]
    pub fn is_positive(&self) -> bool {
        self.is_sign_positive()
    }

    /// Returns `true` if the sign bit of the decimal is negative.
    #[inline(always)]
    pub const fn is_sign_negative(&self) -> bool {
        self.flags & SIGN_MASK > 0
    }

    /// Returns `true` if the sign bit of the decimal is positive.
    #[inline(always)]
    #[must_use]
    pub const fn is_sign_positive(&self) -> bool {
        self.flags & SIGN_MASK == 0
    }

    /// Returns the minimum possible number that `Decimal` can represent.
    #[deprecated(since = "1.12.0", note = "Use the associated constant Decimal::MIN")]
    #[must_use]
    pub const fn min_value() -> Decimal {
        MIN
    }

    /// Returns the maximum possible number that `Decimal` can represent.
    #[deprecated(since = "1.12.0", note = "Use the associated constant Decimal::MAX")]
    #[must_use]
    pub const fn max_value() -> Decimal {
        MAX
    }

    /// Returns a new `Decimal` integral with no fractional portion.
    /// This is a true truncation whereby no rounding is performed.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let pi = Decimal::new(3141, 3);
    /// let trunc = Decimal::new(3, 0);
    /// // note that it returns a decimal
    /// assert_eq!(pi.trunc(), trunc);
    /// ```
    #[must_use]
    pub fn trunc(&self) -> Decimal {
        let mut scale = self.scale();
        if scale == 0 {
            // Nothing to do
            return *self;
        }
        let mut working = [self.lo, self.mid, self.hi];
        while scale > 0 {
            // We're removing precision, so we don't care about overflow
            if scale < 10 {
                ops::array::div_by_u32(&mut working, POWERS_10[scale as usize]);
                break;
            } else {
                ops::array::div_by_u32(&mut working, POWERS_10[9]);
                // Only 9 as this array starts with 1
                scale -= 9;
            }
        }
        Decimal {
            lo: working[0],
            mid: working[1],
            hi: working[2],
            flags: flags(self.is_sign_negative(), 0),
        }
    }

    /// Returns a new `Decimal` representing the fractional portion of the number.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let pi = Decimal::new(3141, 3);
    /// let fract = Decimal::new(141, 3);
    /// // note that it returns a decimal
    /// assert_eq!(pi.fract(), fract);
    /// ```
    #[must_use]
    pub fn fract(&self) -> Decimal {
        // This is essentially the original number minus the integral.
        // Could possibly be optimized in the future
        *self - self.trunc()
    }

    /// Computes the absolute value of `self`.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let num = Decimal::new(-3141, 3);
    /// assert_eq!(num.abs().to_string(), "3.141");
    /// ```
    #[must_use]
    pub fn abs(&self) -> Decimal {
        let mut me = *self;
        me.set_sign_positive(true);
        me
    }

    /// Returns the largest integer less than or equal to a number.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let num = Decimal::new(3641, 3);
    /// assert_eq!(num.floor().to_string(), "3");
    /// ```
    #[must_use]
    pub fn floor(&self) -> Decimal {
        let scale = self.scale();
        if scale == 0 {
            // Nothing to do
            return *self;
        }

        // Opportunity for optimization here
        let floored = self.trunc();
        if self.is_sign_negative() && !self.fract().is_zero() {
            floored - ONE
        } else {
            floored
        }
    }

    /// Returns the smallest integer greater than or equal to a number.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let num = Decimal::new(3141, 3);
    /// assert_eq!(num.ceil().to_string(), "4");
    /// let num = Decimal::new(3, 0);
    /// assert_eq!(num.ceil().to_string(), "3");
    /// ```
    #[must_use]
    pub fn ceil(&self) -> Decimal {
        let scale = self.scale();
        if scale == 0 {
            // Nothing to do
            return *self;
        }

        // Opportunity for optimization here
        if self.is_sign_positive() && !self.fract().is_zero() {
            self.trunc() + ONE
        } else {
            self.trunc()
        }
    }

    /// Returns the maximum of the two numbers.
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let x = Decimal::new(1, 0);
    /// let y = Decimal::new(2, 0);
    /// assert_eq!(y, x.max(y));
    /// ```
    #[must_use]
    pub fn max(self, other: Decimal) -> Decimal {
        if self < other {
            other
        } else {
            self
        }
    }

    /// Returns the minimum of the two numbers.
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// let x = Decimal::new(1, 0);
    /// let y = Decimal::new(2, 0);
    /// assert_eq!(x, x.min(y));
    /// ```
    #[must_use]
    pub fn min(self, other: Decimal) -> Decimal {
        if self > other {
            other
        } else {
            self
        }
    }

    /// Strips any trailing zero's from a `Decimal` and converts -0 to 0.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::prelude::*;
    ///
    /// let number = Decimal::from_str("3.100").unwrap();
    /// assert_eq!(number.normalize().to_string(), "3.1");
    /// ```
    #[must_use]
    pub fn normalize(&self) -> Decimal {
        let mut result = *self;
        result.normalize_assign();
        result
    }

    /// An in place version of `normalize`. Strips any trailing zero's from a `Decimal` and converts -0 to 0.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::prelude::*;
    ///
    /// let mut number = Decimal::from_str("3.100").unwrap();
    /// assert_eq!(number.to_string(), "3.100");
    /// number.normalize_assign();
    /// assert_eq!(number.to_string(), "3.1");
    /// ```
    pub fn normalize_assign(&mut self) {
        if self.is_zero() {
            self.flags = 0;
            return;
        }

        let mut scale = self.scale();
        if scale == 0 {
            return;
        }

        let mut result = self.mantissa_array3();
        let mut working = self.mantissa_array3();
        while scale > 0 {
            if ops::array::div_by_u32(&mut working, 10) > 0 {
                break;
            }
            scale -= 1;
            result.copy_from_slice(&working);
        }
        self.lo = result[0];
        self.mid = result[1];
        self.hi = result[2];
        self.flags = flags(self.is_sign_negative(), scale);
    }

    /// Returns a new `Decimal` number with no fractional portion (i.e. an integer).
    /// Rounding currently follows "Bankers Rounding" rules. e.g. 6.5 -> 6, 7.5 -> 8
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    ///
    /// // Demonstrating bankers rounding...
    /// let number_down = Decimal::new(65, 1);
    /// let number_up   = Decimal::new(75, 1);
    /// assert_eq!(number_down.round().to_string(), "6");
    /// assert_eq!(number_up.round().to_string(), "8");
    /// ```
    #[must_use]
    pub fn round(&self) -> Decimal {
        self.round_dp(0)
    }

    /// Returns a new `Decimal` number with the specified number of decimal points for fractional
    /// portion.
    /// Rounding is performed using the provided [`RoundingStrategy`]
    ///
    /// # Arguments
    /// * `dp`: the number of decimal points to round to.
    /// * `strategy`: the [`RoundingStrategy`] to use.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::{Decimal, RoundingStrategy};
    /// use core::str::FromStr;
    ///
    /// let tax = Decimal::from_str("3.4395").unwrap();
    /// assert_eq!(tax.round_dp_with_strategy(2, RoundingStrategy::MidpointAwayFromZero).to_string(), "3.44");
    /// ```
    #[must_use]
    pub fn round_dp_with_strategy(&self, dp: u32, strategy: RoundingStrategy) -> Decimal {
        // Short circuit for zero
        if self.is_zero() {
            return Decimal {
                lo: 0,
                mid: 0,
                hi: 0,
                flags: flags(self.is_sign_negative(), dp),
            };
        }

        let old_scale = self.scale();

        // return early if decimal has a smaller number of fractional places than dp
        // e.g. 2.51 rounded to 3 decimal places is 2.51
        if old_scale <= dp {
            return *self;
        }

        let mut value = [self.lo, self.mid, self.hi];
        let mut value_scale = self.scale();
        let negative = self.is_sign_negative();

        value_scale -= dp;

        // Rescale to zero so it's easier to work with
        while value_scale > 0 {
            if value_scale < 10 {
                ops::array::div_by_u32(&mut value, POWERS_10[value_scale as usize]);
                value_scale = 0;
            } else {
                ops::array::div_by_u32(&mut value, POWERS_10[9]);
                value_scale -= 9;
            }
        }

        // Do some midpoint rounding checks
        // We're actually doing two things here.
        //  1. Figuring out midpoint rounding when we're right on the boundary. e.g. 2.50000
        //  2. Figuring out whether to add one or not e.g. 2.51
        // For this, we need to figure out the fractional portion that is additional to
        // the rounded number. e.g. for 0.12345 rounding to 2dp we'd want 345.
        // We're doing the equivalent of losing precision (e.g. to get 0.12)
        // then increasing the precision back up to 0.12000
        let mut offset = [self.lo, self.mid, self.hi];
        let mut diff = old_scale - dp;

        while diff > 0 {
            if diff < 10 {
                ops::array::div_by_u32(&mut offset, POWERS_10[diff as usize]);
                break;
            } else {
                ops::array::div_by_u32(&mut offset, POWERS_10[9]);
                // Only 9 as this array starts with 1
                diff -= 9;
            }
        }

        let mut diff = old_scale - dp;

        while diff > 0 {
            if diff < 10 {
                ops::array::mul_by_u32(&mut offset, POWERS_10[diff as usize]);
                break;
            } else {
                ops::array::mul_by_u32(&mut offset, POWERS_10[9]);
                // Only 9 as this array starts with 1
                diff -= 9;
            }
        }

        let mut decimal_portion = [self.lo, self.mid, self.hi];
        ops::array::sub_by_internal(&mut decimal_portion, &offset);

        // If the decimal_portion is zero then we round based on the other data
        let mut cap = [5, 0, 0];
        for _ in 0..(old_scale - dp - 1) {
            ops::array::mul_by_u32(&mut cap, 10);
        }
        let order = ops::array::cmp_internal(&decimal_portion, &cap);

        #[allow(deprecated)]
        match strategy {
            RoundingStrategy::BankersRounding | RoundingStrategy::MidpointNearestEven => {
                match order {
                    Ordering::Equal => {
                        if (value[0] & 1) == 1 {
                            ops::array::add_one_internal(&mut value);
                        }
                    }
                    Ordering::Greater => {
                        // Doesn't matter about the decimal portion
                        ops::array::add_one_internal(&mut value);
                    }
                    _ => {}
                }
            }
            RoundingStrategy::RoundHalfDown | RoundingStrategy::MidpointTowardZero => {
                if let Ordering::Greater = order {
                    ops::array::add_one_internal(&mut value);
                }
            }
            RoundingStrategy::RoundHalfUp | RoundingStrategy::MidpointAwayFromZero => {
                // when Ordering::Equal, decimal_portion is 0.5 exactly
                // when Ordering::Greater, decimal_portion is > 0.5
                match order {
                    Ordering::Equal => {
                        ops::array::add_one_internal(&mut value);
                    }
                    Ordering::Greater => {
                        // Doesn't matter about the decimal portion
                        ops::array::add_one_internal(&mut value);
                    }
                    _ => {}
                }
            }
            RoundingStrategy::RoundUp | RoundingStrategy::AwayFromZero => {
                if !ops::array::is_all_zero(&decimal_portion) {
                    ops::array::add_one_internal(&mut value);
                }
            }
            RoundingStrategy::ToPositiveInfinity => {
                if !negative && !ops::array::is_all_zero(&decimal_portion) {
                    ops::array::add_one_internal(&mut value);
                }
            }
            RoundingStrategy::ToNegativeInfinity => {
                if negative && !ops::array::is_all_zero(&decimal_portion) {
                    ops::array::add_one_internal(&mut value);
                }
            }
            RoundingStrategy::RoundDown | RoundingStrategy::ToZero => (),
        }

        Decimal::from_parts(value[0], value[1], value[2], negative, dp)
    }

    /// Returns a new `Decimal` number with the specified number of decimal points for fractional portion.
    /// Rounding currently follows "Bankers Rounding" rules. e.g. 6.5 -> 6, 7.5 -> 8
    ///
    /// # Arguments
    /// * `dp`: the number of decimal points to round to.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    /// use core::str::FromStr;
    ///
    /// let pi = Decimal::from_str("3.1415926535897932384626433832").unwrap();
    /// assert_eq!(pi.round_dp(2).to_string(), "3.14");
    /// ```
    #[must_use]
    pub fn round_dp(&self, dp: u32) -> Decimal {
        self.round_dp_with_strategy(dp, RoundingStrategy::MidpointNearestEven)
    }

    /// Convert `Decimal` to an internal representation of the underlying struct. This is useful
    /// for debugging the internal state of the object.
    ///
    /// # Important Disclaimer
    /// This is primarily intended for library maintainers. The internal representation of a
    /// `Decimal` is considered "unstable" for public use.
    ///
    /// # Example
    ///
    /// ```
    /// use rust_decimal::Decimal;
    /// use core::str::FromStr;
    ///
    /// let pi = Decimal::from_str("3.1415926535897932384626433832").unwrap();
    /// assert_eq!(format!("{:?}", pi), "3.1415926535897932384626433832");
    /// assert_eq!(format!("{:?}", pi.unpack()), "UnpackedDecimal { \
    ///     negative: false, scale: 28, hi: 1703060790, mid: 185874565, lo: 1102470952 \
    /// }");
    /// ```
    pub const fn unpack(&self) -> UnpackedDecimal {
        UnpackedDecimal {
            negative: self.is_sign_negative(),
            scale: self.scale(),
            hi: self.hi,
            lo: self.lo,
            mid: self.mid,
        }
    }

    #[inline(always)]
    pub(crate) const fn lo(&self) -> u32 {
        self.lo
    }

    #[inline(always)]
    pub(crate) const fn mid(&self) -> u32 {
        self.mid
    }

    #[inline(always)]
    pub(crate) const fn hi(&self) -> u32 {
        self.hi
    }

    #[inline(always)]
    pub(crate) const fn flags(&self) -> u32 {
        self.flags
    }

    #[inline(always)]
    pub(crate) const fn mantissa_array3(&self) -> [u32; 3] {
        [self.lo, self.mid, self.hi]
    }

    #[inline(always)]
    pub(crate) const fn mantissa_array4(&self) -> [u32; 4] {
        [self.lo, self.mid, self.hi, 0]
    }

    fn base2_to_decimal(bits: &mut [u32; 3], exponent2: i32, positive: bool, is64: bool) -> Option<Self> {
        // 2^exponent2 = (10^exponent2)/(5^exponent2)
        //             = (5^-exponent2)*(10^exponent2)
        let mut exponent5 = -exponent2;
        let mut exponent10 = exponent2; // Ultimately, we want this for the scale

        while exponent5 > 0 {
            // Check to see if the mantissa is divisible by 2
            if bits[0] & 0x1 == 0 {
                exponent10 += 1;
                exponent5 -= 1;

                // We can divide by 2 without losing precision
                let hi_carry = bits[2] & 0x1 == 1;
                bits[2] >>= 1;
                let mid_carry = bits[1] & 0x1 == 1;
                bits[1] = (bits[1] >> 1) | if hi_carry { SIGN_MASK } else { 0 };
                bits[0] = (bits[0] >> 1) | if mid_carry { SIGN_MASK } else { 0 };
            } else {
                // The mantissa is NOT divisible by 2. Therefore the mantissa should
                // be multiplied by 5, unless the multiplication overflows.
                exponent5 -= 1;

                let mut temp = [bits[0], bits[1], bits[2]];
                if ops::array::mul_by_u32(&mut temp, 5) == 0 {
                    // Multiplication succeeded without overflow, so copy result back
                    bits[0] = temp[0];
                    bits[1] = temp[1];
                    bits[2] = temp[2];
                } else {
                    // Multiplication by 5 overflows. The mantissa should be divided
                    // by 2, and therefore will lose significant digits.
                    exponent10 += 1;

                    // Shift right
                    let hi_carry = bits[2] & 0x1 == 1;
                    bits[2] >>= 1;
                    let mid_carry = bits[1] & 0x1 == 1;
                    bits[1] = (bits[1] >> 1) | if hi_carry { SIGN_MASK } else { 0 };
                    bits[0] = (bits[0] >> 1) | if mid_carry { SIGN_MASK } else { 0 };
                }
            }
        }

        // In order to divide the value by 5, it is best to multiply by 2/10.
        // Therefore, exponent10 is decremented, and the mantissa should be multiplied by 2
        while exponent5 < 0 {
            if bits[2] & SIGN_MASK == 0 {
                // No far left bit, the mantissa can withstand a shift-left without overflowing
                exponent10 -= 1;
                exponent5 += 1;
                ops::array::shl1_internal(bits, 0);
            } else {
                // The mantissa would overflow if shifted. Therefore it should be
                // directly divided by 5. This will lose significant digits, unless
                // by chance the mantissa happens to be divisible by 5.
                exponent5 += 1;
                ops::array::div_by_u32(bits, 5);
            }
        }

        // At this point, the mantissa has assimilated the exponent5, but
        // exponent10 might not be suitable for assignment. exponent10 must be
        // in the range [-MAX_PRECISION..0], so the mantissa must be scaled up or
        // down appropriately.
        while exponent10 > 0 {
            // In order to bring exponent10 down to 0, the mantissa should be
            // multiplied by 10 to compensate. If the exponent10 is too big, this
            // will cause the mantissa to overflow.
            if ops::array::mul_by_u32(bits, 10) == 0 {
                exponent10 -= 1;
            } else {
                // Overflowed - return?
                return None;
            }
        }

        // In order to bring exponent up to -MAX_PRECISION, the mantissa should
        // be divided by 10 to compensate. If the exponent10 is too small, this
        // will cause the mantissa to underflow and become 0.
        while exponent10 < -(MAX_PRECISION as i32) {
            let rem10 = ops::array::div_by_u32(bits, 10);
            exponent10 += 1;
            if ops::array::is_all_zero(bits) {
                // Underflow, unable to keep dividing
                exponent10 = 0;
            } else if rem10 >= 5 {
                ops::array::add_one_internal(bits);
            }
        }

        // This step is required in order to remove excess bits of precision from the
        // end of the bit representation, down to the precision guaranteed by the
        // floating point number
        if is64 {
            // Guaranteed to about 16 dp
            while exponent10 < 0 && (bits[2] != 0 || (bits[1] & 0xFFF0_0000) != 0) {
                let rem10 = ops::array::div_by_u32(bits, 10);
                exponent10 += 1;
                if rem10 >= 5 {
                    ops::array::add_one_internal(bits);
                }
            }
        } else {
            // Guaranteed to about 7 dp
            while exponent10 < 0 && ((bits[0] & 0xFF00_0000) != 0 || bits[1] != 0 || bits[2] != 0) {
                let rem10 = ops::array::div_by_u32(bits, 10);
                exponent10 += 1;
                if rem10 >= 5 {
                    ops::array::add_one_internal(bits);
                }
            }
        }

        // Remove multiples of 10 from the representation
        while exponent10 < 0 {
            let mut temp = [bits[0], bits[1], bits[2]];
            let remainder = ops::array::div_by_u32(&mut temp, 10);
            if remainder == 0 {
                exponent10 += 1;
                bits[0] = temp[0];
                bits[1] = temp[1];
                bits[2] = temp[2];
            } else {
                break;
            }
        }

        Some(Decimal {
            lo: bits[0],
            mid: bits[1],
            hi: bits[2],
            flags: flags(!positive, -exponent10 as u32),
        })
    }

    /// Checked addition. Computes `self + other`, returning `None` if overflow occurred.
    #[inline(always)]
    #[must_use]
    pub fn checked_add(self, other: Decimal) -> Option<Decimal> {
        match ops::add_impl(&self, &other) {
            CalculationResult::Ok(result) => Some(result),
            CalculationResult::Overflow => None,
            _ => None,
        }
    }

    /// Checked subtraction. Computes `self - other`, returning `None` if overflow occurred.
    #[inline(always)]
    #[must_use]
    pub fn checked_sub(self, other: Decimal) -> Option<Decimal> {
        match ops::sub_impl(&self, &other) {
            CalculationResult::Ok(result) => Some(result),
            CalculationResult::Overflow => None,
            _ => None,
        }
    }

    /// Checked multiplication. Computes `self * other`, returning `None` if overflow occurred.
    #[inline]
    #[must_use]
    pub fn checked_mul(self, other: Decimal) -> Option<Decimal> {
        match ops::mul_impl(&self, &other) {
            CalculationResult::Ok(result) => Some(result),
            CalculationResult::Overflow => None,
            _ => None,
        }
    }

    /// Checked division. Computes `self / other`, returning `None` if `other == 0.0` or the
    /// division results in overflow.
    #[inline]
    #[must_use]
    pub fn checked_div(self, other: Decimal) -> Option<Decimal> {
        match ops::div_impl(&self, &other) {
            CalculationResult::Ok(quot) => Some(quot),
            CalculationResult::Overflow => None,
            CalculationResult::DivByZero => None,
        }
    }

    /// Checked remainder. Computes `self % other`, returning `None` if `other == 0.0`.
    #[inline]
    #[must_use]
    pub fn checked_rem(self, other: Decimal) -> Option<Decimal> {
        match ops::rem_impl(&self, &other) {
            CalculationResult::Ok(quot) => Some(quot),
            CalculationResult::Overflow => None,
            CalculationResult::DivByZero => None,
        }
    }

    pub fn from_str_radix(str: &str, radix: u32) -> Result<Self, crate::Error> {
        if radix == 10 {
            crate::str::parse_str_radix_10(str)
        } else {
            crate::str::parse_str_radix_n(str, radix)
        }
    }
}

impl Default for Decimal {
    fn default() -> Self {
        ZERO
    }
}

pub(crate) enum CalculationResult {
    Ok(Decimal),
    Overflow,
    DivByZero,
}

#[inline]
const fn flags(neg: bool, scale: u32) -> u32 {
    (scale << SCALE_SHIFT) | ((neg as u32) << SIGN_SHIFT)
}

macro_rules! impl_from {
    ($T:ty, $from_ty:path) => {
        impl core::convert::From<$T> for Decimal {
            #[inline]
            fn from(t: $T) -> Self {
                $from_ty(t).unwrap()
            }
        }
    };
}
impl_from!(isize, FromPrimitive::from_isize);
impl_from!(i8, FromPrimitive::from_i8);
impl_from!(i16, FromPrimitive::from_i16);
impl_from!(i32, FromPrimitive::from_i32);
impl_from!(i64, FromPrimitive::from_i64);
impl_from!(usize, FromPrimitive::from_usize);
impl_from!(u8, FromPrimitive::from_u8);
impl_from!(u16, FromPrimitive::from_u16);
impl_from!(u32, FromPrimitive::from_u32);
impl_from!(u64, FromPrimitive::from_u64);

impl_from!(i128, FromPrimitive::from_i128);
impl_from!(u128, FromPrimitive::from_u128);

macro_rules! forward_val_val_binop {
    (impl $imp:ident for $res:ty, $method:ident) => {
        impl $imp<$res> for $res {
            type Output = $res;

            #[inline]
            fn $method(self, other: $res) -> $res {
                (&self).$method(&other)
            }
        }
    };
}

macro_rules! forward_ref_val_binop {
    (impl $imp:ident for $res:ty, $method:ident) => {
        impl<'a> $imp<$res> for &'a $res {
            type Output = $res;

            #[inline]
            fn $method(self, other: $res) -> $res {
                self.$method(&other)
            }
        }
    };
}

macro_rules! forward_val_ref_binop {
    (impl $imp:ident for $res:ty, $method:ident) => {
        impl<'a> $imp<&'a $res> for $res {
            type Output = $res;

            #[inline]
            fn $method(self, other: &$res) -> $res {
                (&self).$method(other)
            }
        }
    };
}

macro_rules! forward_all_binop {
    (impl $imp:ident for $res:ty, $method:ident) => {
        forward_val_val_binop!(impl $imp for $res, $method);
        forward_ref_val_binop!(impl $imp for $res, $method);
        forward_val_ref_binop!(impl $imp for $res, $method);
    };
}

impl Zero for Decimal {
    fn zero() -> Decimal {
        ZERO
    }

    fn is_zero(&self) -> bool {
        self.is_zero()
    }
}

impl One for Decimal {
    fn one() -> Decimal {
        ONE
    }
}

impl Signed for Decimal {
    fn abs(&self) -> Self {
        self.abs()
    }

    fn abs_sub(&self, other: &Self) -> Self {
        if self <= other {
            ZERO
        } else {
            self.abs()
        }
    }

    fn signum(&self) -> Self {
        if self.is_zero() {
            ZERO
        } else {
            let mut value = ONE;
            if self.is_sign_negative() {
                value.set_sign_negative(true);
            }
            value
        }
    }

    fn is_positive(&self) -> bool {
        self.is_sign_positive()
    }

    fn is_negative(&self) -> bool {
        self.is_sign_negative()
    }
}

impl CheckedAdd for Decimal {
    #[inline]
    fn checked_add(&self, v: &Decimal) -> Option<Decimal> {
        Decimal::checked_add(*self, *v)
    }
}

impl CheckedSub for Decimal {
    #[inline]
    fn checked_sub(&self, v: &Decimal) -> Option<Decimal> {
        Decimal::checked_sub(*self, *v)
    }
}

impl CheckedMul for Decimal {
    #[inline]
    fn checked_mul(&self, v: &Decimal) -> Option<Decimal> {
        Decimal::checked_mul(*self, *v)
    }
}

impl CheckedDiv for Decimal {
    #[inline]
    fn checked_div(&self, v: &Decimal) -> Option<Decimal> {
        Decimal::checked_div(*self, *v)
    }
}

impl CheckedRem for Decimal {
    #[inline]
    fn checked_rem(&self, v: &Decimal) -> Option<Decimal> {
        Decimal::checked_rem(*self, *v)
    }
}

impl Num for Decimal {
    type FromStrRadixErr = Error;

    fn from_str_radix(str: &str, radix: u32) -> Result<Self, Self::FromStrRadixErr> {
        Decimal::from_str_radix(str, radix)
    }
}

impl FromStr for Decimal {
    type Err = Error;

    fn from_str(value: &str) -> Result<Decimal, Self::Err> {
        crate::str::parse_str_radix_10(value)
    }
}

impl FromPrimitive for Decimal {
    fn from_i32(n: i32) -> Option<Decimal> {
        let flags: u32;
        let value_copy: i64;
        if n >= 0 {
            flags = 0;
            value_copy = n as i64;
        } else {
            flags = SIGN_MASK;
            value_copy = -(n as i64);
        }
        Some(Decimal {
            flags,
            lo: value_copy as u32,
            mid: 0,
            hi: 0,
        })
    }

    fn from_i64(n: i64) -> Option<Decimal> {
        let flags: u32;
        let value_copy: i128;
        if n >= 0 {
            flags = 0;
            value_copy = n as i128;
        } else {
            flags = SIGN_MASK;
            value_copy = -(n as i128);
        }
        Some(Decimal {
            flags,
            lo: value_copy as u32,
            mid: (value_copy >> 32) as u32,
            hi: 0,
        })
    }

    fn from_i128(n: i128) -> Option<Decimal> {
        let flags;
        let unsigned;
        if n >= 0 {
            unsigned = n as u128;
            flags = 0;
        } else {
            unsigned = -n as u128;
            flags = SIGN_MASK;
        };
        // Check if we overflow
        if unsigned >> 96 != 0 {
            return None;
        }
        Some(Decimal {
            flags,
            lo: unsigned as u32,
            mid: (unsigned >> 32) as u32,
            hi: (unsigned >> 64) as u32,
        })
    }

    fn from_u32(n: u32) -> Option<Decimal> {
        Some(Decimal {
            flags: 0,
            lo: n,
            mid: 0,
            hi: 0,
        })
    }

    fn from_u64(n: u64) -> Option<Decimal> {
        Some(Decimal {
            flags: 0,
            lo: n as u32,
            mid: (n >> 32) as u32,
            hi: 0,
        })
    }

    fn from_u128(n: u128) -> Option<Decimal> {
        // Check if we overflow
        if n >> 96 != 0 {
            return None;
        }
        Some(Decimal {
            flags: 0,
            lo: n as u32,
            mid: (n >> 32) as u32,
            hi: (n >> 64) as u32,
        })
    }

    fn from_f32(n: f32) -> Option<Decimal> {
        // Handle the case if it is NaN, Infinity or -Infinity
        if !n.is_finite() {
            return None;
        }

        // It's a shame we can't use a union for this due to it being broken up by bits
        // i.e. 1/8/23 (sign, exponent, mantissa)
        // See https://en.wikipedia.org/wiki/IEEE_754-1985
        // n = (sign*-1) * 2^exp * mantissa
        // Decimal of course stores this differently... 10^-exp * significand
        let raw = n.to_bits();
        let positive = (raw >> 31) == 0;
        let biased_exponent = ((raw >> 23) & 0xFF) as i32;
        let mantissa = raw & 0x007F_FFFF;

        // Handle the special zero case
        if biased_exponent == 0 && mantissa == 0 {
            let mut zero = ZERO;
            if !positive {
                zero.set_sign_negative(true);
            }
            return Some(zero);
        }

        // Get the bits and exponent2
        let mut exponent2 = biased_exponent - 127;
        let mut bits = [mantissa, 0u32, 0u32];
        if biased_exponent == 0 {
            // Denormalized number - correct the exponent
            exponent2 += 1;
        } else {
            // Add extra hidden bit to mantissa
            bits[0] |= 0x0080_0000;
        }

        // The act of copying a mantissa as integer bits is equivalent to shifting
        // left the mantissa 23 bits. The exponent is reduced to compensate.
        exponent2 -= 23;

        // Convert to decimal
        Decimal::base2_to_decimal(&mut bits, exponent2, positive, false)
    }

    fn from_f64(n: f64) -> Option<Decimal> {
        // Handle the case if it is NaN, Infinity or -Infinity
        if !n.is_finite() {
            return None;
        }

        // It's a shame we can't use a union for this due to it being broken up by bits
        // i.e. 1/11/52 (sign, exponent, mantissa)
        // See https://en.wikipedia.org/wiki/IEEE_754-1985
        // n = (sign*-1) * 2^exp * mantissa
        // Decimal of course stores this differently... 10^-exp * significand
        let raw = n.to_bits();
        let positive = (raw >> 63) == 0;
        let biased_exponent = ((raw >> 52) & 0x7FF) as i32;
        let mantissa = raw & 0x000F_FFFF_FFFF_FFFF;

        // Handle the special zero case
        if biased_exponent == 0 && mantissa == 0 {
            let mut zero = ZERO;
            if !positive {
                zero.set_sign_negative(true);
            }
            return Some(zero);
        }

        // Get the bits and exponent2
        let mut exponent2 = biased_exponent - 1023;
        let mut bits = [
            (mantissa & 0xFFFF_FFFF) as u32,
            ((mantissa >> 32) & 0xFFFF_FFFF) as u32,
            0u32,
        ];
        if biased_exponent == 0 {
            // Denormalized number - correct the exponent
            exponent2 += 1;
        } else {
            // Add extra hidden bit to mantissa
            bits[1] |= 0x0010_0000;
        }

        // The act of copying a mantissa as integer bits is equivalent to shifting
        // left the mantissa 52 bits. The exponent is reduced to compensate.
        exponent2 -= 52;

        // Convert to decimal
        Decimal::base2_to_decimal(&mut bits, exponent2, positive, true)
    }
}

impl ToPrimitive for Decimal {
    fn to_i64(&self) -> Option<i64> {
        let d = self.trunc();
        // Quick overflow check
        if d.hi != 0 || (d.mid & 0x8000_0000) > 0 {
            // Overflow
            return None;
        }

        let raw: i64 = (i64::from(d.mid) << 32) | i64::from(d.lo);
        if self.is_sign_negative() {
            Some(-raw)
        } else {
            Some(raw)
        }
    }

    fn to_i128(&self) -> Option<i128> {
        let d = self.trunc();
        let raw: i128 = ((i128::from(d.hi) << 64) | i128::from(d.mid) << 32) | i128::from(d.lo);
        if self.is_sign_negative() {
            Some(-raw)
        } else {
            Some(raw)
        }
    }

    fn to_u64(&self) -> Option<u64> {
        if self.is_sign_negative() {
            return None;
        }

        let d = self.trunc();
        if d.hi != 0 {
            // Overflow
            return None;
        }

        Some((u64::from(d.mid) << 32) | u64::from(d.lo))
    }

    fn to_u128(&self) -> Option<u128> {
        if self.is_sign_negative() {
            return None;
        }

        let d = self.trunc();
        Some((u128::from(d.hi) << 64) | (u128::from(d.mid) << 32) | u128::from(d.lo))
    }

    fn to_f64(&self) -> Option<f64> {
        if self.scale() == 0 {
            let integer = self.to_i64();
            integer.map(|i| i as f64)
        } else {
            let sign: f64 = if self.is_sign_negative() { -1.0 } else { 1.0 };
            let mut mantissa: u128 = self.lo.into();
            mantissa |= (self.mid as u128) << 32;
            mantissa |= (self.hi as u128) << 64;
            // scale is at most 28, so this fits comfortably into a u128.
            let scale = self.scale();
            let precision: u128 = 10_u128.pow(scale);
            let integral_part = mantissa / precision;
            let frac_part = mantissa % precision;
            let frac_f64 = (frac_part as f64) / (precision as f64);
            let value = sign * ((integral_part as f64) + frac_f64);
            let round_to = 10f64.powi(self.scale() as i32);
            Some(value * round_to / round_to)
        }
    }
}

impl core::convert::TryFrom<f32> for Decimal {
    type Error = crate::Error;

    fn try_from(value: f32) -> Result<Self, Error> {
        Self::from_f32(value).ok_or_else(|| Error::from("Failed to convert to Decimal"))
    }
}

impl core::convert::TryFrom<f64> for Decimal {
    type Error = crate::Error;

    fn try_from(value: f64) -> Result<Self, Error> {
        Self::from_f64(value).ok_or_else(|| Error::from("Failed to convert to Decimal"))
    }
}

impl core::convert::TryFrom<Decimal> for f32 {
    type Error = crate::Error;

    fn try_from(value: Decimal) -> Result<Self, Self::Error> {
        Decimal::to_f32(&value).ok_or_else(|| Error::from("Failed to convert to f32"))
    }
}

impl core::convert::TryFrom<Decimal> for f64 {
    type Error = crate::Error;

    fn try_from(value: Decimal) -> Result<Self, Self::Error> {
        Decimal::to_f64(&value).ok_or_else(|| Error::from("Failed to convert to f64"))
    }
}

impl fmt::Display for Decimal {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        let rep = crate::str::to_str_internal(self, false, f.precision());
        f.pad_integral(self.is_sign_positive(), "", rep.as_str())
    }
}

impl fmt::Debug for Decimal {
    fn fmt(&self, f: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        fmt::Display::fmt(self, f)
    }
}

impl fmt::LowerExp for Decimal {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        crate::str::fmt_scientific_notation(self, "e", f)
    }
}

impl fmt::UpperExp for Decimal {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        crate::str::fmt_scientific_notation(self, "E", f)
    }
}

impl Neg for Decimal {
    type Output = Decimal;

    fn neg(self) -> Decimal {
        let mut copy = self;
        copy.set_sign_negative(self.is_sign_positive());
        copy
    }
}

impl<'a> Neg for &'a Decimal {
    type Output = Decimal;

    fn neg(self) -> Decimal {
        Decimal {
            flags: flags(!self.is_sign_negative(), self.scale()),
            hi: self.hi,
            lo: self.lo,
            mid: self.mid,
        }
    }
}

forward_all_binop!(impl Add for Decimal, add);

impl<'a, 'b> Add<&'b Decimal> for &'a Decimal {
    type Output = Decimal;

    #[inline(always)]
    fn add(self, other: &Decimal) -> Decimal {
        match ops::add_impl(&self, other) {
            CalculationResult::Ok(sum) => sum,
            _ => panic!("Addition overflowed"),
        }
    }
}

impl AddAssign for Decimal {
    fn add_assign(&mut self, other: Decimal) {
        let result = self.add(other);
        self.lo = result.lo;
        self.mid = result.mid;
        self.hi = result.hi;
        self.flags = result.flags;
    }
}

impl<'a> AddAssign<&'a Decimal> for Decimal {
    fn add_assign(&mut self, other: &'a Decimal) {
        Decimal::add_assign(self, *other)
    }
}

impl<'a> AddAssign<Decimal> for &'a mut Decimal {
    fn add_assign(&mut self, other: Decimal) {
        Decimal::add_assign(*self, other)
    }
}

impl<'a> AddAssign<&'a Decimal> for &'a mut Decimal {
    fn add_assign(&mut self, other: &'a Decimal) {
        Decimal::add_assign(*self, *other)
    }
}

forward_all_binop!(impl Sub for Decimal, sub);

impl<'a, 'b> Sub<&'b Decimal> for &'a Decimal {
    type Output = Decimal;

    #[inline(always)]
    fn sub(self, other: &Decimal) -> Decimal {
        match ops::sub_impl(&self, other) {
            CalculationResult::Ok(sum) => sum,
            _ => panic!("Subtraction overflowed"),
        }
    }
}

impl SubAssign for Decimal {
    fn sub_assign(&mut self, other: Decimal) {
        let result = self.sub(other);
        self.lo = result.lo;
        self.mid = result.mid;
        self.hi = result.hi;
        self.flags = result.flags;
    }
}

impl<'a> SubAssign<&'a Decimal> for Decimal {
    fn sub_assign(&mut self, other: &'a Decimal) {
        Decimal::sub_assign(self, *other)
    }
}

impl<'a> SubAssign<Decimal> for &'a mut Decimal {
    fn sub_assign(&mut self, other: Decimal) {
        Decimal::sub_assign(*self, other)
    }
}

impl<'a> SubAssign<&'a Decimal> for &'a mut Decimal {
    fn sub_assign(&mut self, other: &'a Decimal) {
        Decimal::sub_assign(*self, *other)
    }
}

forward_all_binop!(impl Mul for Decimal, mul);

impl<'a, 'b> Mul<&'b Decimal> for &'a Decimal {
    type Output = Decimal;

    #[inline]
    fn mul(self, other: &Decimal) -> Decimal {
        match ops::mul_impl(&self, other) {
            CalculationResult::Ok(prod) => prod,
            _ => panic!("Multiplication overflowed"),
        }
    }
}

impl MulAssign for Decimal {
    fn mul_assign(&mut self, other: Decimal) {
        let result = self.mul(other);
        self.lo = result.lo;
        self.mid = result.mid;
        self.hi = result.hi;
        self.flags = result.flags;
    }
}

impl<'a> MulAssign<&'a Decimal> for Decimal {
    fn mul_assign(&mut self, other: &'a Decimal) {
        Decimal::mul_assign(self, *other)
    }
}

impl<'a> MulAssign<Decimal> for &'a mut Decimal {
    fn mul_assign(&mut self, other: Decimal) {
        Decimal::mul_assign(*self, other)
    }
}

impl<'a> MulAssign<&'a Decimal> for &'a mut Decimal {
    fn mul_assign(&mut self, other: &'a Decimal) {
        Decimal::mul_assign(*self, *other)
    }
}

forward_all_binop!(impl Div for Decimal, div);

impl<'a, 'b> Div<&'b Decimal> for &'a Decimal {
    type Output = Decimal;

    fn div(self, other: &Decimal) -> Decimal {
        match ops::div_impl(&self, other) {
            CalculationResult::Ok(quot) => quot,
            CalculationResult::Overflow => panic!("Division overflowed"),
            CalculationResult::DivByZero => panic!("Division by zero"),
        }
    }
}

impl DivAssign for Decimal {
    fn div_assign(&mut self, other: Decimal) {
        let result = self.div(other);
        self.lo = result.lo;
        self.mid = result.mid;
        self.hi = result.hi;
        self.flags = result.flags;
    }
}

impl<'a> DivAssign<&'a Decimal> for Decimal {
    fn div_assign(&mut self, other: &'a Decimal) {
        Decimal::div_assign(self, *other)
    }
}

impl<'a> DivAssign<Decimal> for &'a mut Decimal {
    fn div_assign(&mut self, other: Decimal) {
        Decimal::div_assign(*self, other)
    }
}

impl<'a> DivAssign<&'a Decimal> for &'a mut Decimal {
    fn div_assign(&mut self, other: &'a Decimal) {
        Decimal::div_assign(*self, *other)
    }
}

forward_all_binop!(impl Rem for Decimal, rem);

impl<'a, 'b> Rem<&'b Decimal> for &'a Decimal {
    type Output = Decimal;

    #[inline]
    fn rem(self, other: &Decimal) -> Decimal {
        match ops::rem_impl(&self, other) {
            CalculationResult::Ok(rem) => rem,
            CalculationResult::Overflow => panic!("Division overflowed"),
            CalculationResult::DivByZero => panic!("Division by zero"),
        }
    }
}

impl RemAssign for Decimal {
    fn rem_assign(&mut self, other: Decimal) {
        let result = self.rem(other);
        self.lo = result.lo;
        self.mid = result.mid;
        self.hi = result.hi;
        self.flags = result.flags;
    }
}

impl<'a> RemAssign<&'a Decimal> for Decimal {
    fn rem_assign(&mut self, other: &'a Decimal) {
        Decimal::rem_assign(self, *other)
    }
}

impl<'a> RemAssign<Decimal> for &'a mut Decimal {
    fn rem_assign(&mut self, other: Decimal) {
        Decimal::rem_assign(*self, other)
    }
}

impl<'a> RemAssign<&'a Decimal> for &'a mut Decimal {
    fn rem_assign(&mut self, other: &'a Decimal) {
        Decimal::rem_assign(*self, *other)
    }
}

impl PartialEq for Decimal {
    #[inline]
    fn eq(&self, other: &Decimal) -> bool {
        self.cmp(other) == Equal
    }
}

impl Eq for Decimal {}

impl Hash for Decimal {
    fn hash<H: Hasher>(&self, state: &mut H) {
        let n = self.normalize();
        n.lo.hash(state);
        n.mid.hash(state);
        n.hi.hash(state);
        n.flags.hash(state);
    }
}

impl PartialOrd for Decimal {
    #[inline]
    fn partial_cmp(&self, other: &Decimal) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for Decimal {
    fn cmp(&self, other: &Decimal) -> Ordering {
        ops::cmp_impl(self, other)
    }
}

impl Sum for Decimal {
    fn sum<I: Iterator<Item = Decimal>>(iter: I) -> Self {
        let mut sum = ZERO;
        for i in iter {
            sum += i;
        }
        sum
    }
}

impl<'a> Sum<&'a Decimal> for Decimal {
    fn sum<I: Iterator<Item = &'a Decimal>>(iter: I) -> Self {
        let mut sum = ZERO;
        for i in iter {
            sum += i;
        }
        sum
    }
}
