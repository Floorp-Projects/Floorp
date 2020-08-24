#[cfg(feature = "quickcheck")]
use crate::std_alloc::Box;
use crate::std_alloc::{Cow, String, Vec};
use core::cmp;
use core::cmp::Ordering::{self, Equal, Greater, Less};
#[cfg(has_try_from)]
use core::convert::TryFrom;
use core::default::Default;
use core::fmt;
use core::hash;
use core::iter::{Product, Sum};
use core::mem;
use core::ops::{
    Add, AddAssign, BitAnd, BitAndAssign, BitOr, BitOrAssign, BitXor, BitXorAssign, Div, DivAssign,
    Mul, MulAssign, Rem, RemAssign, Shl, ShlAssign, Shr, ShrAssign, Sub, SubAssign,
};
use core::str::{self, FromStr};
use core::{f32, f64};
use core::{u32, u64, u8};

#[cfg(feature = "serde")]
use serde;

use num_integer::{Integer, Roots};
use num_traits::float::FloatCore;
use num_traits::{
    CheckedAdd, CheckedDiv, CheckedMul, CheckedSub, FromPrimitive, Num, One, Pow, ToPrimitive,
    Unsigned, Zero,
};

use crate::big_digit::{self, BigDigit};

#[path = "algorithms.rs"]
mod algorithms;
#[path = "monty.rs"]
mod monty;

use self::algorithms::{__add2, __sub2rev, add2, sub2, sub2rev};
use self::algorithms::{biguint_shl, biguint_shr};
use self::algorithms::{cmp_slice, fls, ilog2};
use self::algorithms::{div_rem, div_rem_digit, div_rem_ref, rem_digit};
use self::algorithms::{mac_with_carry, mul3, scalar_mul};
use self::monty::monty_modpow;

use crate::UsizePromotion;

use crate::ParseBigIntError;
#[cfg(has_try_from)]
use crate::TryFromBigIntError;

#[cfg(feature = "quickcheck")]
use quickcheck::{Arbitrary, Gen};

/// A big unsigned integer type.
#[derive(Debug)]
pub struct BigUint {
    data: Vec<BigDigit>,
}

// Note: derived `Clone` doesn't specialize `clone_from`,
// but we want to keep the allocation in `data`.
impl Clone for BigUint {
    #[inline]
    fn clone(&self) -> Self {
        BigUint {
            data: self.data.clone(),
        }
    }

    #[inline]
    fn clone_from(&mut self, other: &Self) {
        self.data.clone_from(&other.data);
    }
}

#[cfg(feature = "quickcheck")]
impl Arbitrary for BigUint {
    fn arbitrary<G: Gen>(g: &mut G) -> Self {
        // Use arbitrary from Vec
        biguint_from_vec(Vec::<BigDigit>::arbitrary(g))
    }

    fn shrink(&self) -> Box<dyn Iterator<Item = Self>> {
        // Use shrinker from Vec
        Box::new(self.data.shrink().map(biguint_from_vec))
    }
}

impl hash::Hash for BigUint {
    #[inline]
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        debug_assert!(self.data.last() != Some(&0));
        self.data.hash(state);
    }
}

impl PartialEq for BigUint {
    #[inline]
    fn eq(&self, other: &BigUint) -> bool {
        debug_assert!(self.data.last() != Some(&0));
        debug_assert!(other.data.last() != Some(&0));
        self.data == other.data
    }
}
impl Eq for BigUint {}

impl PartialOrd for BigUint {
    #[inline]
    fn partial_cmp(&self, other: &BigUint) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for BigUint {
    #[inline]
    fn cmp(&self, other: &BigUint) -> Ordering {
        cmp_slice(&self.data[..], &other.data[..])
    }
}

impl Default for BigUint {
    #[inline]
    fn default() -> BigUint {
        Zero::zero()
    }
}

impl fmt::Display for BigUint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.pad_integral(true, "", &self.to_str_radix(10))
    }
}

impl fmt::LowerHex for BigUint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.pad_integral(true, "0x", &self.to_str_radix(16))
    }
}

impl fmt::UpperHex for BigUint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let mut s = self.to_str_radix(16);
        s.make_ascii_uppercase();
        f.pad_integral(true, "0x", &s)
    }
}

impl fmt::Binary for BigUint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.pad_integral(true, "0b", &self.to_str_radix(2))
    }
}

impl fmt::Octal for BigUint {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.pad_integral(true, "0o", &self.to_str_radix(8))
    }
}

impl FromStr for BigUint {
    type Err = ParseBigIntError;

    #[inline]
    fn from_str(s: &str) -> Result<BigUint, ParseBigIntError> {
        BigUint::from_str_radix(s, 10)
    }
}

// Convert from a power of two radix (bits == ilog2(radix)) where bits evenly divides
// BigDigit::BITS
fn from_bitwise_digits_le(v: &[u8], bits: u8) -> BigUint {
    debug_assert!(!v.is_empty() && bits <= 8 && big_digit::BITS % bits == 0);
    debug_assert!(v.iter().all(|&c| BigDigit::from(c) < (1 << bits)));

    let digits_per_big_digit = big_digit::BITS / bits;

    let data = v
        .chunks(digits_per_big_digit.into())
        .map(|chunk| {
            chunk
                .iter()
                .rev()
                .fold(0, |acc, &c| (acc << bits) | BigDigit::from(c))
        })
        .collect();

    biguint_from_vec(data)
}

// Convert from a power of two radix (bits == ilog2(radix)) where bits doesn't evenly divide
// BigDigit::BITS
fn from_inexact_bitwise_digits_le(v: &[u8], bits: u8) -> BigUint {
    debug_assert!(!v.is_empty() && bits <= 8 && big_digit::BITS % bits != 0);
    debug_assert!(v.iter().all(|&c| BigDigit::from(c) < (1 << bits)));

    let big_digits = (v.len() as u64)
        .saturating_mul(bits.into())
        .div_ceil(&big_digit::BITS.into())
        .to_usize()
        .unwrap_or(core::usize::MAX);
    let mut data = Vec::with_capacity(big_digits);

    let mut d = 0;
    let mut dbits = 0; // number of bits we currently have in d

    // walk v accumululating bits in d; whenever we accumulate big_digit::BITS in d, spit out a
    // big_digit:
    for &c in v {
        d |= BigDigit::from(c) << dbits;
        dbits += bits;

        if dbits >= big_digit::BITS {
            data.push(d);
            dbits -= big_digit::BITS;
            // if dbits was > big_digit::BITS, we dropped some of the bits in c (they couldn't fit
            // in d) - grab the bits we lost here:
            d = BigDigit::from(c) >> (bits - dbits);
        }
    }

    if dbits > 0 {
        debug_assert!(dbits < big_digit::BITS);
        data.push(d as BigDigit);
    }

    biguint_from_vec(data)
}

// Read little-endian radix digits
fn from_radix_digits_be(v: &[u8], radix: u32) -> BigUint {
    debug_assert!(!v.is_empty() && !radix.is_power_of_two());
    debug_assert!(v.iter().all(|&c| u32::from(c) < radix));

    #[cfg(feature = "std")]
    let radix_log2 = f64::from(radix).log2();
    #[cfg(not(feature = "std"))]
    let radix_log2 = ilog2(radix.next_power_of_two()) as f64;

    // Estimate how big the result will be, so we can pre-allocate it.
    let bits = radix_log2 * v.len() as f64;
    let big_digits = (bits / big_digit::BITS as f64).ceil();
    let mut data = Vec::with_capacity(big_digits.to_usize().unwrap_or(0));

    let (base, power) = get_radix_base(radix, big_digit::BITS);
    let radix = radix as BigDigit;

    let r = v.len() % power;
    let i = if r == 0 { power } else { r };
    let (head, tail) = v.split_at(i);

    let first = head
        .iter()
        .fold(0, |acc, &d| acc * radix + BigDigit::from(d));
    data.push(first);

    debug_assert!(tail.len() % power == 0);
    for chunk in tail.chunks(power) {
        if data.last() != Some(&0) {
            data.push(0);
        }

        let mut carry = 0;
        for d in data.iter_mut() {
            *d = mac_with_carry(0, *d, base, &mut carry);
        }
        debug_assert!(carry == 0);

        let n = chunk
            .iter()
            .fold(0, |acc, &d| acc * radix + BigDigit::from(d));
        add2(&mut data, &[n]);
    }

    biguint_from_vec(data)
}

impl Num for BigUint {
    type FromStrRadixErr = ParseBigIntError;

    /// Creates and initializes a `BigUint`.
    fn from_str_radix(s: &str, radix: u32) -> Result<BigUint, ParseBigIntError> {
        assert!(2 <= radix && radix <= 36, "The radix must be within 2...36");
        let mut s = s;
        if s.starts_with('+') {
            let tail = &s[1..];
            if !tail.starts_with('+') {
                s = tail
            }
        }

        if s.is_empty() {
            return Err(ParseBigIntError::empty());
        }

        if s.starts_with('_') {
            // Must lead with a real digit!
            return Err(ParseBigIntError::invalid());
        }

        // First normalize all characters to plain digit values
        let mut v = Vec::with_capacity(s.len());
        for b in s.bytes() {
            let d = match b {
                b'0'..=b'9' => b - b'0',
                b'a'..=b'z' => b - b'a' + 10,
                b'A'..=b'Z' => b - b'A' + 10,
                b'_' => continue,
                _ => u8::MAX,
            };
            if d < radix as u8 {
                v.push(d);
            } else {
                return Err(ParseBigIntError::invalid());
            }
        }

        let res = if radix.is_power_of_two() {
            // Powers of two can use bitwise masks and shifting instead of multiplication
            let bits = ilog2(radix);
            v.reverse();
            if big_digit::BITS % bits == 0 {
                from_bitwise_digits_le(&v, bits)
            } else {
                from_inexact_bitwise_digits_le(&v, bits)
            }
        } else {
            from_radix_digits_be(&v, radix)
        };
        Ok(res)
    }
}

forward_val_val_binop!(impl BitAnd for BigUint, bitand);
forward_ref_val_binop!(impl BitAnd for BigUint, bitand);

// do not use forward_ref_ref_binop_commutative! for bitand so that we can
// clone the smaller value rather than the larger, avoiding over-allocation
impl<'a, 'b> BitAnd<&'b BigUint> for &'a BigUint {
    type Output = BigUint;

    #[inline]
    fn bitand(self, other: &BigUint) -> BigUint {
        // forward to val-ref, choosing the smaller to clone
        if self.data.len() <= other.data.len() {
            self.clone() & other
        } else {
            other.clone() & self
        }
    }
}

forward_val_assign!(impl BitAndAssign for BigUint, bitand_assign);

impl<'a> BitAnd<&'a BigUint> for BigUint {
    type Output = BigUint;

    #[inline]
    fn bitand(mut self, other: &BigUint) -> BigUint {
        self &= other;
        self
    }
}
impl<'a> BitAndAssign<&'a BigUint> for BigUint {
    #[inline]
    fn bitand_assign(&mut self, other: &BigUint) {
        for (ai, &bi) in self.data.iter_mut().zip(other.data.iter()) {
            *ai &= bi;
        }
        self.data.truncate(other.data.len());
        self.normalize();
    }
}

forward_all_binop_to_val_ref_commutative!(impl BitOr for BigUint, bitor);
forward_val_assign!(impl BitOrAssign for BigUint, bitor_assign);

impl<'a> BitOr<&'a BigUint> for BigUint {
    type Output = BigUint;

    fn bitor(mut self, other: &BigUint) -> BigUint {
        self |= other;
        self
    }
}
impl<'a> BitOrAssign<&'a BigUint> for BigUint {
    #[inline]
    fn bitor_assign(&mut self, other: &BigUint) {
        for (ai, &bi) in self.data.iter_mut().zip(other.data.iter()) {
            *ai |= bi;
        }
        if other.data.len() > self.data.len() {
            let extra = &other.data[self.data.len()..];
            self.data.extend(extra.iter().cloned());
        }
    }
}

forward_all_binop_to_val_ref_commutative!(impl BitXor for BigUint, bitxor);
forward_val_assign!(impl BitXorAssign for BigUint, bitxor_assign);

impl<'a> BitXor<&'a BigUint> for BigUint {
    type Output = BigUint;

    fn bitxor(mut self, other: &BigUint) -> BigUint {
        self ^= other;
        self
    }
}
impl<'a> BitXorAssign<&'a BigUint> for BigUint {
    #[inline]
    fn bitxor_assign(&mut self, other: &BigUint) {
        for (ai, &bi) in self.data.iter_mut().zip(other.data.iter()) {
            *ai ^= bi;
        }
        if other.data.len() > self.data.len() {
            let extra = &other.data[self.data.len()..];
            self.data.extend(extra.iter().cloned());
        }
        self.normalize();
    }
}

macro_rules! impl_shift {
    (@ref $Shx:ident :: $shx:ident, $ShxAssign:ident :: $shx_assign:ident, $rhs:ty) => {
        impl<'b> $Shx<&'b $rhs> for BigUint {
            type Output = BigUint;

            #[inline]
            fn $shx(self, rhs: &'b $rhs) -> BigUint {
                $Shx::$shx(self, *rhs)
            }
        }
        impl<'a, 'b> $Shx<&'b $rhs> for &'a BigUint {
            type Output = BigUint;

            #[inline]
            fn $shx(self, rhs: &'b $rhs) -> BigUint {
                $Shx::$shx(self, *rhs)
            }
        }
        impl<'b> $ShxAssign<&'b $rhs> for BigUint {
            #[inline]
            fn $shx_assign(&mut self, rhs: &'b $rhs) {
                $ShxAssign::$shx_assign(self, *rhs);
            }
        }
    };
    ($($rhs:ty),+) => {$(
        impl Shl<$rhs> for BigUint {
            type Output = BigUint;

            #[inline]
            fn shl(self, rhs: $rhs) -> BigUint {
                biguint_shl(Cow::Owned(self), rhs)
            }
        }
        impl<'a> Shl<$rhs> for &'a BigUint {
            type Output = BigUint;

            #[inline]
            fn shl(self, rhs: $rhs) -> BigUint {
                biguint_shl(Cow::Borrowed(self), rhs)
            }
        }
        impl ShlAssign<$rhs> for BigUint {
            #[inline]
            fn shl_assign(&mut self, rhs: $rhs) {
                let n = mem::replace(self, BigUint::zero());
                *self = n << rhs;
            }
        }
        impl_shift! { @ref Shl::shl, ShlAssign::shl_assign, $rhs }

        impl Shr<$rhs> for BigUint {
            type Output = BigUint;

            #[inline]
            fn shr(self, rhs: $rhs) -> BigUint {
                biguint_shr(Cow::Owned(self), rhs)
            }
        }
        impl<'a> Shr<$rhs> for &'a BigUint {
            type Output = BigUint;

            #[inline]
            fn shr(self, rhs: $rhs) -> BigUint {
                biguint_shr(Cow::Borrowed(self), rhs)
            }
        }
        impl ShrAssign<$rhs> for BigUint {
            #[inline]
            fn shr_assign(&mut self, rhs: $rhs) {
                let n = mem::replace(self, BigUint::zero());
                *self = n >> rhs;
            }
        }
        impl_shift! { @ref Shr::shr, ShrAssign::shr_assign, $rhs }
    )*};
}

impl_shift! { u8, u16, u32, u64, u128, usize }
impl_shift! { i8, i16, i32, i64, i128, isize }

impl Zero for BigUint {
    #[inline]
    fn zero() -> BigUint {
        BigUint { data: Vec::new() }
    }

    #[inline]
    fn set_zero(&mut self) {
        self.data.clear();
    }

    #[inline]
    fn is_zero(&self) -> bool {
        self.data.is_empty()
    }
}

impl One for BigUint {
    #[inline]
    fn one() -> BigUint {
        BigUint { data: vec![1] }
    }

    #[inline]
    fn set_one(&mut self) {
        self.data.clear();
        self.data.push(1);
    }

    #[inline]
    fn is_one(&self) -> bool {
        self.data[..] == [1]
    }
}

impl Unsigned for BigUint {}

impl<'b> Pow<&'b BigUint> for BigUint {
    type Output = BigUint;

    #[inline]
    fn pow(self, exp: &BigUint) -> BigUint {
        if self.is_one() || exp.is_zero() {
            BigUint::one()
        } else if self.is_zero() {
            BigUint::zero()
        } else if let Some(exp) = exp.to_u64() {
            self.pow(exp)
        } else if let Some(exp) = exp.to_u128() {
            self.pow(exp)
        } else {
            // At this point, `self >= 2` and `exp >= 2¹²⁸`. The smallest possible result given
            // `2.pow(2¹²⁸)` would require far more memory than 64-bit targets can address!
            panic!("memory overflow")
        }
    }
}

impl Pow<BigUint> for BigUint {
    type Output = BigUint;

    #[inline]
    fn pow(self, exp: BigUint) -> BigUint {
        Pow::pow(self, &exp)
    }
}

impl<'a, 'b> Pow<&'b BigUint> for &'a BigUint {
    type Output = BigUint;

    #[inline]
    fn pow(self, exp: &BigUint) -> BigUint {
        if self.is_one() || exp.is_zero() {
            BigUint::one()
        } else if self.is_zero() {
            BigUint::zero()
        } else {
            self.clone().pow(exp)
        }
    }
}

impl<'a> Pow<BigUint> for &'a BigUint {
    type Output = BigUint;

    #[inline]
    fn pow(self, exp: BigUint) -> BigUint {
        Pow::pow(self, &exp)
    }
}

macro_rules! pow_impl {
    ($T:ty) => {
        impl Pow<$T> for BigUint {
            type Output = BigUint;

            fn pow(self, mut exp: $T) -> BigUint {
                if exp == 0 {
                    return BigUint::one();
                }
                let mut base = self;

                while exp & 1 == 0 {
                    base = &base * &base;
                    exp >>= 1;
                }

                if exp == 1 {
                    return base;
                }

                let mut acc = base.clone();
                while exp > 1 {
                    exp >>= 1;
                    base = &base * &base;
                    if exp & 1 == 1 {
                        acc = &acc * &base;
                    }
                }
                acc
            }
        }

        impl<'b> Pow<&'b $T> for BigUint {
            type Output = BigUint;

            #[inline]
            fn pow(self, exp: &$T) -> BigUint {
                Pow::pow(self, *exp)
            }
        }

        impl<'a> Pow<$T> for &'a BigUint {
            type Output = BigUint;

            #[inline]
            fn pow(self, exp: $T) -> BigUint {
                if exp == 0 {
                    return BigUint::one();
                }
                Pow::pow(self.clone(), exp)
            }
        }

        impl<'a, 'b> Pow<&'b $T> for &'a BigUint {
            type Output = BigUint;

            #[inline]
            fn pow(self, exp: &$T) -> BigUint {
                Pow::pow(self, *exp)
            }
        }
    };
}

pow_impl!(u8);
pow_impl!(u16);
pow_impl!(u32);
pow_impl!(u64);
pow_impl!(usize);
pow_impl!(u128);

forward_all_binop_to_val_ref_commutative!(impl Add for BigUint, add);
forward_val_assign!(impl AddAssign for BigUint, add_assign);

impl<'a> Add<&'a BigUint> for BigUint {
    type Output = BigUint;

    fn add(mut self, other: &BigUint) -> BigUint {
        self += other;
        self
    }
}
impl<'a> AddAssign<&'a BigUint> for BigUint {
    #[inline]
    fn add_assign(&mut self, other: &BigUint) {
        let self_len = self.data.len();
        let carry = if self_len < other.data.len() {
            let lo_carry = __add2(&mut self.data[..], &other.data[..self_len]);
            self.data.extend_from_slice(&other.data[self_len..]);
            __add2(&mut self.data[self_len..], &[lo_carry])
        } else {
            __add2(&mut self.data[..], &other.data[..])
        };
        if carry != 0 {
            self.data.push(carry);
        }
    }
}

promote_unsigned_scalars!(impl Add for BigUint, add);
promote_unsigned_scalars_assign!(impl AddAssign for BigUint, add_assign);
forward_all_scalar_binop_to_val_val_commutative!(impl Add<u32> for BigUint, add);
forward_all_scalar_binop_to_val_val_commutative!(impl Add<u64> for BigUint, add);
forward_all_scalar_binop_to_val_val_commutative!(impl Add<u128> for BigUint, add);

impl Add<u32> for BigUint {
    type Output = BigUint;

    #[inline]
    fn add(mut self, other: u32) -> BigUint {
        self += other;
        self
    }
}

impl AddAssign<u32> for BigUint {
    #[inline]
    fn add_assign(&mut self, other: u32) {
        if other != 0 {
            if self.data.is_empty() {
                self.data.push(0);
            }

            let carry = __add2(&mut self.data, &[other as BigDigit]);
            if carry != 0 {
                self.data.push(carry);
            }
        }
    }
}

impl Add<u64> for BigUint {
    type Output = BigUint;

    #[inline]
    fn add(mut self, other: u64) -> BigUint {
        self += other;
        self
    }
}

impl AddAssign<u64> for BigUint {
    #[cfg(not(u64_digit))]
    #[inline]
    fn add_assign(&mut self, other: u64) {
        let (hi, lo) = big_digit::from_doublebigdigit(other);
        if hi == 0 {
            *self += lo;
        } else {
            while self.data.len() < 2 {
                self.data.push(0);
            }

            let carry = __add2(&mut self.data, &[lo, hi]);
            if carry != 0 {
                self.data.push(carry);
            }
        }
    }

    #[cfg(u64_digit)]
    #[inline]
    fn add_assign(&mut self, other: u64) {
        if other != 0 {
            if self.data.is_empty() {
                self.data.push(0);
            }

            let carry = __add2(&mut self.data, &[other as BigDigit]);
            if carry != 0 {
                self.data.push(carry);
            }
        }
    }
}

impl Add<u128> for BigUint {
    type Output = BigUint;

    #[inline]
    fn add(mut self, other: u128) -> BigUint {
        self += other;
        self
    }
}

impl AddAssign<u128> for BigUint {
    #[cfg(not(u64_digit))]
    #[inline]
    fn add_assign(&mut self, other: u128) {
        if other <= u128::from(u64::max_value()) {
            *self += other as u64
        } else {
            let (a, b, c, d) = u32_from_u128(other);
            let carry = if a > 0 {
                while self.data.len() < 4 {
                    self.data.push(0);
                }
                __add2(&mut self.data, &[d, c, b, a])
            } else {
                debug_assert!(b > 0);
                while self.data.len() < 3 {
                    self.data.push(0);
                }
                __add2(&mut self.data, &[d, c, b])
            };

            if carry != 0 {
                self.data.push(carry);
            }
        }
    }

    #[cfg(u64_digit)]
    #[inline]
    fn add_assign(&mut self, other: u128) {
        let (hi, lo) = big_digit::from_doublebigdigit(other);
        if hi == 0 {
            *self += lo;
        } else {
            while self.data.len() < 2 {
                self.data.push(0);
            }

            let carry = __add2(&mut self.data, &[lo, hi]);
            if carry != 0 {
                self.data.push(carry);
            }
        }
    }
}

forward_val_val_binop!(impl Sub for BigUint, sub);
forward_ref_ref_binop!(impl Sub for BigUint, sub);
forward_val_assign!(impl SubAssign for BigUint, sub_assign);

impl<'a> Sub<&'a BigUint> for BigUint {
    type Output = BigUint;

    fn sub(mut self, other: &BigUint) -> BigUint {
        self -= other;
        self
    }
}
impl<'a> SubAssign<&'a BigUint> for BigUint {
    fn sub_assign(&mut self, other: &'a BigUint) {
        sub2(&mut self.data[..], &other.data[..]);
        self.normalize();
    }
}

impl<'a> Sub<BigUint> for &'a BigUint {
    type Output = BigUint;

    fn sub(self, mut other: BigUint) -> BigUint {
        let other_len = other.data.len();
        if other_len < self.data.len() {
            let lo_borrow = __sub2rev(&self.data[..other_len], &mut other.data);
            other.data.extend_from_slice(&self.data[other_len..]);
            if lo_borrow != 0 {
                sub2(&mut other.data[other_len..], &[1])
            }
        } else {
            sub2rev(&self.data[..], &mut other.data[..]);
        }
        other.normalized()
    }
}

promote_unsigned_scalars!(impl Sub for BigUint, sub);
promote_unsigned_scalars_assign!(impl SubAssign for BigUint, sub_assign);
forward_all_scalar_binop_to_val_val!(impl Sub<u32> for BigUint, sub);
forward_all_scalar_binop_to_val_val!(impl Sub<u64> for BigUint, sub);
forward_all_scalar_binop_to_val_val!(impl Sub<u128> for BigUint, sub);

impl Sub<u32> for BigUint {
    type Output = BigUint;

    #[inline]
    fn sub(mut self, other: u32) -> BigUint {
        self -= other;
        self
    }
}

impl SubAssign<u32> for BigUint {
    fn sub_assign(&mut self, other: u32) {
        sub2(&mut self.data[..], &[other as BigDigit]);
        self.normalize();
    }
}

impl Sub<BigUint> for u32 {
    type Output = BigUint;

    #[cfg(not(u64_digit))]
    #[inline]
    fn sub(self, mut other: BigUint) -> BigUint {
        if other.data.len() == 0 {
            other.data.push(self);
        } else {
            sub2rev(&[self], &mut other.data[..]);
        }
        other.normalized()
    }

    #[cfg(u64_digit)]
    #[inline]
    fn sub(self, mut other: BigUint) -> BigUint {
        if other.data.is_empty() {
            other.data.push(self as BigDigit);
        } else {
            sub2rev(&[self as BigDigit], &mut other.data[..]);
        }
        other.normalized()
    }
}

impl Sub<u64> for BigUint {
    type Output = BigUint;

    #[inline]
    fn sub(mut self, other: u64) -> BigUint {
        self -= other;
        self
    }
}

impl SubAssign<u64> for BigUint {
    #[cfg(not(u64_digit))]
    #[inline]
    fn sub_assign(&mut self, other: u64) {
        let (hi, lo) = big_digit::from_doublebigdigit(other);
        sub2(&mut self.data[..], &[lo, hi]);
        self.normalize();
    }

    #[cfg(u64_digit)]
    #[inline]
    fn sub_assign(&mut self, other: u64) {
        sub2(&mut self.data[..], &[other as BigDigit]);
        self.normalize();
    }
}

impl Sub<BigUint> for u64 {
    type Output = BigUint;

    #[cfg(not(u64_digit))]
    #[inline]
    fn sub(self, mut other: BigUint) -> BigUint {
        while other.data.len() < 2 {
            other.data.push(0);
        }

        let (hi, lo) = big_digit::from_doublebigdigit(self);
        sub2rev(&[lo, hi], &mut other.data[..]);
        other.normalized()
    }

    #[cfg(u64_digit)]
    #[inline]
    fn sub(self, mut other: BigUint) -> BigUint {
        if other.data.is_empty() {
            other.data.push(self);
        } else {
            sub2rev(&[self], &mut other.data[..]);
        }
        other.normalized()
    }
}

impl Sub<u128> for BigUint {
    type Output = BigUint;

    #[inline]
    fn sub(mut self, other: u128) -> BigUint {
        self -= other;
        self
    }
}

impl SubAssign<u128> for BigUint {
    #[cfg(not(u64_digit))]
    #[inline]
    fn sub_assign(&mut self, other: u128) {
        let (a, b, c, d) = u32_from_u128(other);
        sub2(&mut self.data[..], &[d, c, b, a]);
        self.normalize();
    }

    #[cfg(u64_digit)]
    #[inline]
    fn sub_assign(&mut self, other: u128) {
        let (hi, lo) = big_digit::from_doublebigdigit(other);
        sub2(&mut self.data[..], &[lo, hi]);
        self.normalize();
    }
}

impl Sub<BigUint> for u128 {
    type Output = BigUint;

    #[cfg(not(u64_digit))]
    #[inline]
    fn sub(self, mut other: BigUint) -> BigUint {
        while other.data.len() < 4 {
            other.data.push(0);
        }

        let (a, b, c, d) = u32_from_u128(self);
        sub2rev(&[d, c, b, a], &mut other.data[..]);
        other.normalized()
    }

    #[cfg(u64_digit)]
    #[inline]
    fn sub(self, mut other: BigUint) -> BigUint {
        while other.data.len() < 2 {
            other.data.push(0);
        }

        let (hi, lo) = big_digit::from_doublebigdigit(self);
        sub2rev(&[lo, hi], &mut other.data[..]);
        other.normalized()
    }
}

forward_all_binop_to_ref_ref!(impl Mul for BigUint, mul);
forward_val_assign!(impl MulAssign for BigUint, mul_assign);

impl<'a, 'b> Mul<&'b BigUint> for &'a BigUint {
    type Output = BigUint;

    #[inline]
    fn mul(self, other: &BigUint) -> BigUint {
        mul3(&self.data[..], &other.data[..])
    }
}
impl<'a> MulAssign<&'a BigUint> for BigUint {
    #[inline]
    fn mul_assign(&mut self, other: &'a BigUint) {
        *self = &*self * other
    }
}

promote_unsigned_scalars!(impl Mul for BigUint, mul);
promote_unsigned_scalars_assign!(impl MulAssign for BigUint, mul_assign);
forward_all_scalar_binop_to_val_val_commutative!(impl Mul<u32> for BigUint, mul);
forward_all_scalar_binop_to_val_val_commutative!(impl Mul<u64> for BigUint, mul);
forward_all_scalar_binop_to_val_val_commutative!(impl Mul<u128> for BigUint, mul);

impl Mul<u32> for BigUint {
    type Output = BigUint;

    #[inline]
    fn mul(mut self, other: u32) -> BigUint {
        self *= other;
        self
    }
}
impl MulAssign<u32> for BigUint {
    #[inline]
    fn mul_assign(&mut self, other: u32) {
        if other == 0 {
            self.data.clear();
        } else {
            let carry = scalar_mul(&mut self.data[..], other as BigDigit);
            if carry != 0 {
                self.data.push(carry);
            }
        }
    }
}

impl Mul<u64> for BigUint {
    type Output = BigUint;

    #[inline]
    fn mul(mut self, other: u64) -> BigUint {
        self *= other;
        self
    }
}
impl MulAssign<u64> for BigUint {
    #[cfg(not(u64_digit))]
    #[inline]
    fn mul_assign(&mut self, other: u64) {
        if other == 0 {
            self.data.clear();
        } else if other <= u64::from(BigDigit::max_value()) {
            *self *= other as BigDigit
        } else {
            let (hi, lo) = big_digit::from_doublebigdigit(other);
            *self = mul3(&self.data[..], &[lo, hi])
        }
    }

    #[cfg(u64_digit)]
    #[inline]
    fn mul_assign(&mut self, other: u64) {
        if other == 0 {
            self.data.clear();
        } else {
            let carry = scalar_mul(&mut self.data[..], other as BigDigit);
            if carry != 0 {
                self.data.push(carry);
            }
        }
    }
}

impl Mul<u128> for BigUint {
    type Output = BigUint;

    #[inline]
    fn mul(mut self, other: u128) -> BigUint {
        self *= other;
        self
    }
}

impl MulAssign<u128> for BigUint {
    #[cfg(not(u64_digit))]
    #[inline]
    fn mul_assign(&mut self, other: u128) {
        if other == 0 {
            self.data.clear();
        } else if other <= u128::from(BigDigit::max_value()) {
            *self *= other as BigDigit
        } else {
            let (a, b, c, d) = u32_from_u128(other);
            *self = mul3(&self.data[..], &[d, c, b, a])
        }
    }

    #[cfg(u64_digit)]
    #[inline]
    fn mul_assign(&mut self, other: u128) {
        if other == 0 {
            self.data.clear();
        } else if other <= BigDigit::max_value() as u128 {
            *self *= other as BigDigit
        } else {
            let (hi, lo) = big_digit::from_doublebigdigit(other);
            *self = mul3(&self.data[..], &[lo, hi])
        }
    }
}

forward_val_ref_binop!(impl Div for BigUint, div);
forward_ref_val_binop!(impl Div for BigUint, div);
forward_val_assign!(impl DivAssign for BigUint, div_assign);

impl Div<BigUint> for BigUint {
    type Output = BigUint;

    #[inline]
    fn div(self, other: BigUint) -> BigUint {
        let (q, _) = div_rem(self, other);
        q
    }
}

impl<'a, 'b> Div<&'b BigUint> for &'a BigUint {
    type Output = BigUint;

    #[inline]
    fn div(self, other: &BigUint) -> BigUint {
        let (q, _) = self.div_rem(other);
        q
    }
}
impl<'a> DivAssign<&'a BigUint> for BigUint {
    #[inline]
    fn div_assign(&mut self, other: &'a BigUint) {
        *self = &*self / other;
    }
}

promote_unsigned_scalars!(impl Div for BigUint, div);
promote_unsigned_scalars_assign!(impl DivAssign for BigUint, div_assign);
forward_all_scalar_binop_to_val_val!(impl Div<u32> for BigUint, div);
forward_all_scalar_binop_to_val_val!(impl Div<u64> for BigUint, div);
forward_all_scalar_binop_to_val_val!(impl Div<u128> for BigUint, div);

impl Div<u32> for BigUint {
    type Output = BigUint;

    #[inline]
    fn div(self, other: u32) -> BigUint {
        let (q, _) = div_rem_digit(self, other as BigDigit);
        q
    }
}
impl DivAssign<u32> for BigUint {
    #[inline]
    fn div_assign(&mut self, other: u32) {
        *self = &*self / other;
    }
}

impl Div<BigUint> for u32 {
    type Output = BigUint;

    #[inline]
    fn div(self, other: BigUint) -> BigUint {
        match other.data.len() {
            0 => panic!("attempt to divide by zero"),
            1 => From::from(self as BigDigit / other.data[0]),
            _ => Zero::zero(),
        }
    }
}

impl Div<u64> for BigUint {
    type Output = BigUint;

    #[inline]
    fn div(self, other: u64) -> BigUint {
        let (q, _) = div_rem(self, From::from(other));
        q
    }
}
impl DivAssign<u64> for BigUint {
    #[inline]
    fn div_assign(&mut self, other: u64) {
        // a vec of size 0 does not allocate, so this is fairly cheap
        let temp = mem::replace(self, Zero::zero());
        *self = temp / other;
    }
}

impl Div<BigUint> for u64 {
    type Output = BigUint;

    #[cfg(not(u64_digit))]
    #[inline]
    fn div(self, other: BigUint) -> BigUint {
        match other.data.len() {
            0 => panic!("attempt to divide by zero"),
            1 => From::from(self / u64::from(other.data[0])),
            2 => From::from(self / big_digit::to_doublebigdigit(other.data[1], other.data[0])),
            _ => Zero::zero(),
        }
    }

    #[cfg(u64_digit)]
    #[inline]
    fn div(self, other: BigUint) -> BigUint {
        match other.data.len() {
            0 => panic!("attempt to divide by zero"),
            1 => From::from(self / other.data[0]),
            _ => Zero::zero(),
        }
    }
}

impl Div<u128> for BigUint {
    type Output = BigUint;

    #[inline]
    fn div(self, other: u128) -> BigUint {
        let (q, _) = div_rem(self, From::from(other));
        q
    }
}

impl DivAssign<u128> for BigUint {
    #[inline]
    fn div_assign(&mut self, other: u128) {
        *self = &*self / other;
    }
}

impl Div<BigUint> for u128 {
    type Output = BigUint;

    #[cfg(not(u64_digit))]
    #[inline]
    fn div(self, other: BigUint) -> BigUint {
        match other.data.len() {
            0 => panic!("attempt to divide by zero"),
            1 => From::from(self / u128::from(other.data[0])),
            2 => From::from(
                self / u128::from(big_digit::to_doublebigdigit(other.data[1], other.data[0])),
            ),
            3 => From::from(self / u32_to_u128(0, other.data[2], other.data[1], other.data[0])),
            4 => From::from(
                self / u32_to_u128(other.data[3], other.data[2], other.data[1], other.data[0]),
            ),
            _ => Zero::zero(),
        }
    }

    #[cfg(u64_digit)]
    #[inline]
    fn div(self, other: BigUint) -> BigUint {
        match other.data.len() {
            0 => panic!("attempt to divide by zero"),
            1 => From::from(self / other.data[0] as u128),
            2 => From::from(self / big_digit::to_doublebigdigit(other.data[1], other.data[0])),
            _ => Zero::zero(),
        }
    }
}

forward_val_ref_binop!(impl Rem for BigUint, rem);
forward_ref_val_binop!(impl Rem for BigUint, rem);
forward_val_assign!(impl RemAssign for BigUint, rem_assign);

impl Rem<BigUint> for BigUint {
    type Output = BigUint;

    #[inline]
    fn rem(self, other: BigUint) -> BigUint {
        if let Some(other) = other.to_u32() {
            &self % other
        } else {
            let (_, r) = div_rem(self, other);
            r
        }
    }
}

impl<'a, 'b> Rem<&'b BigUint> for &'a BigUint {
    type Output = BigUint;

    #[inline]
    fn rem(self, other: &BigUint) -> BigUint {
        if let Some(other) = other.to_u32() {
            self % other
        } else {
            let (_, r) = self.div_rem(other);
            r
        }
    }
}
impl<'a> RemAssign<&'a BigUint> for BigUint {
    #[inline]
    fn rem_assign(&mut self, other: &BigUint) {
        *self = &*self % other;
    }
}

promote_unsigned_scalars!(impl Rem for BigUint, rem);
promote_unsigned_scalars_assign!(impl RemAssign for BigUint, rem_assign);
forward_all_scalar_binop_to_ref_val!(impl Rem<u32> for BigUint, rem);
forward_all_scalar_binop_to_val_val!(impl Rem<u64> for BigUint, rem);
forward_all_scalar_binop_to_val_val!(impl Rem<u128> for BigUint, rem);

impl<'a> Rem<u32> for &'a BigUint {
    type Output = BigUint;

    #[inline]
    fn rem(self, other: u32) -> BigUint {
        rem_digit(self, other as BigDigit).into()
    }
}
impl RemAssign<u32> for BigUint {
    #[inline]
    fn rem_assign(&mut self, other: u32) {
        *self = &*self % other;
    }
}

impl<'a> Rem<&'a BigUint> for u32 {
    type Output = BigUint;

    #[inline]
    fn rem(mut self, other: &'a BigUint) -> BigUint {
        self %= other;
        From::from(self)
    }
}

macro_rules! impl_rem_assign_scalar {
    ($scalar:ty, $to_scalar:ident) => {
        forward_val_assign_scalar!(impl RemAssign for BigUint, $scalar, rem_assign);
        impl<'a> RemAssign<&'a BigUint> for $scalar {
            #[inline]
            fn rem_assign(&mut self, other: &BigUint) {
                *self = match other.$to_scalar() {
                    None => *self,
                    Some(0) => panic!("attempt to divide by zero"),
                    Some(v) => *self % v
                };
            }
        }
    }
}

// we can scalar %= BigUint for any scalar, including signed types
impl_rem_assign_scalar!(u128, to_u128);
impl_rem_assign_scalar!(usize, to_usize);
impl_rem_assign_scalar!(u64, to_u64);
impl_rem_assign_scalar!(u32, to_u32);
impl_rem_assign_scalar!(u16, to_u16);
impl_rem_assign_scalar!(u8, to_u8);
impl_rem_assign_scalar!(i128, to_i128);
impl_rem_assign_scalar!(isize, to_isize);
impl_rem_assign_scalar!(i64, to_i64);
impl_rem_assign_scalar!(i32, to_i32);
impl_rem_assign_scalar!(i16, to_i16);
impl_rem_assign_scalar!(i8, to_i8);

impl Rem<u64> for BigUint {
    type Output = BigUint;

    #[inline]
    fn rem(self, other: u64) -> BigUint {
        let (_, r) = div_rem(self, From::from(other));
        r
    }
}
impl RemAssign<u64> for BigUint {
    #[inline]
    fn rem_assign(&mut self, other: u64) {
        *self = &*self % other;
    }
}

impl Rem<BigUint> for u64 {
    type Output = BigUint;

    #[inline]
    fn rem(mut self, other: BigUint) -> BigUint {
        self %= other;
        From::from(self)
    }
}

impl Rem<u128> for BigUint {
    type Output = BigUint;

    #[inline]
    fn rem(self, other: u128) -> BigUint {
        let (_, r) = div_rem(self, From::from(other));
        r
    }
}

impl RemAssign<u128> for BigUint {
    #[inline]
    fn rem_assign(&mut self, other: u128) {
        *self = &*self % other;
    }
}

impl Rem<BigUint> for u128 {
    type Output = BigUint;

    #[inline]
    fn rem(mut self, other: BigUint) -> BigUint {
        self %= other;
        From::from(self)
    }
}

impl CheckedAdd for BigUint {
    #[inline]
    fn checked_add(&self, v: &BigUint) -> Option<BigUint> {
        Some(self.add(v))
    }
}

impl CheckedSub for BigUint {
    #[inline]
    fn checked_sub(&self, v: &BigUint) -> Option<BigUint> {
        match self.cmp(v) {
            Less => None,
            Equal => Some(Zero::zero()),
            Greater => Some(self.sub(v)),
        }
    }
}

impl CheckedMul for BigUint {
    #[inline]
    fn checked_mul(&self, v: &BigUint) -> Option<BigUint> {
        Some(self.mul(v))
    }
}

impl CheckedDiv for BigUint {
    #[inline]
    fn checked_div(&self, v: &BigUint) -> Option<BigUint> {
        if v.is_zero() {
            return None;
        }
        Some(self.div(v))
    }
}

impl Integer for BigUint {
    #[inline]
    fn div_rem(&self, other: &BigUint) -> (BigUint, BigUint) {
        div_rem_ref(self, other)
    }

    #[inline]
    fn div_floor(&self, other: &BigUint) -> BigUint {
        let (d, _) = div_rem_ref(self, other);
        d
    }

    #[inline]
    fn mod_floor(&self, other: &BigUint) -> BigUint {
        let (_, m) = div_rem_ref(self, other);
        m
    }

    #[inline]
    fn div_mod_floor(&self, other: &BigUint) -> (BigUint, BigUint) {
        div_rem_ref(self, other)
    }

    #[inline]
    fn div_ceil(&self, other: &BigUint) -> BigUint {
        let (d, m) = div_rem_ref(self, other);
        if m.is_zero() {
            d
        } else {
            d + 1u32
        }
    }

    /// Calculates the Greatest Common Divisor (GCD) of the number and `other`.
    ///
    /// The result is always positive.
    #[inline]
    fn gcd(&self, other: &Self) -> Self {
        #[inline]
        fn twos(x: &BigUint) -> u64 {
            x.trailing_zeros().unwrap_or(0)
        }

        // Stein's algorithm
        if self.is_zero() {
            return other.clone();
        }
        if other.is_zero() {
            return self.clone();
        }
        let mut m = self.clone();
        let mut n = other.clone();

        // find common factors of 2
        let shift = cmp::min(twos(&n), twos(&m));

        // divide m and n by 2 until odd
        // m inside loop
        n >>= twos(&n);

        while !m.is_zero() {
            m >>= twos(&m);
            if n > m {
                mem::swap(&mut n, &mut m)
            }
            m -= &n;
        }

        n << shift
    }

    /// Calculates the Lowest Common Multiple (LCM) of the number and `other`.
    #[inline]
    fn lcm(&self, other: &BigUint) -> BigUint {
        if self.is_zero() && other.is_zero() {
            Self::zero()
        } else {
            self / self.gcd(other) * other
        }
    }

    /// Calculates the Greatest Common Divisor (GCD) and
    /// Lowest Common Multiple (LCM) together.
    #[inline]
    fn gcd_lcm(&self, other: &Self) -> (Self, Self) {
        let gcd = self.gcd(other);
        let lcm = if gcd.is_zero() {
            Self::zero()
        } else {
            self / &gcd * other
        };
        (gcd, lcm)
    }

    /// Deprecated, use `is_multiple_of` instead.
    #[inline]
    fn divides(&self, other: &BigUint) -> bool {
        self.is_multiple_of(other)
    }

    /// Returns `true` if the number is a multiple of `other`.
    #[inline]
    fn is_multiple_of(&self, other: &BigUint) -> bool {
        (self % other).is_zero()
    }

    /// Returns `true` if the number is divisible by `2`.
    #[inline]
    fn is_even(&self) -> bool {
        // Considering only the last digit.
        match self.data.first() {
            Some(x) => x.is_even(),
            None => true,
        }
    }

    /// Returns `true` if the number is not divisible by `2`.
    #[inline]
    fn is_odd(&self) -> bool {
        !self.is_even()
    }

    /// Rounds up to nearest multiple of argument.
    #[inline]
    fn next_multiple_of(&self, other: &Self) -> Self {
        let m = self.mod_floor(other);
        if m.is_zero() {
            self.clone()
        } else {
            self + (other - m)
        }
    }
    /// Rounds down to nearest multiple of argument.
    #[inline]
    fn prev_multiple_of(&self, other: &Self) -> Self {
        self - self.mod_floor(other)
    }
}

#[inline]
fn fixpoint<F>(mut x: BigUint, max_bits: u64, f: F) -> BigUint
where
    F: Fn(&BigUint) -> BigUint,
{
    let mut xn = f(&x);

    // If the value increased, then the initial guess must have been low.
    // Repeat until we reverse course.
    while x < xn {
        // Sometimes an increase will go way too far, especially with large
        // powers, and then take a long time to walk back.  We know an upper
        // bound based on bit size, so saturate on that.
        x = if xn.bits() > max_bits {
            BigUint::one() << max_bits
        } else {
            xn
        };
        xn = f(&x);
    }

    // Now keep repeating while the estimate is decreasing.
    while x > xn {
        x = xn;
        xn = f(&x);
    }
    x
}

impl Roots for BigUint {
    // nth_root, sqrt and cbrt use Newton's method to compute
    // principal root of a given degree for a given integer.

    // Reference:
    // Brent & Zimmermann, Modern Computer Arithmetic, v0.5.9, Algorithm 1.14
    fn nth_root(&self, n: u32) -> Self {
        assert!(n > 0, "root degree n must be at least 1");

        if self.is_zero() || self.is_one() {
            return self.clone();
        }

        match n {
            // Optimize for small n
            1 => return self.clone(),
            2 => return self.sqrt(),
            3 => return self.cbrt(),
            _ => (),
        }

        // The root of non-zero values less than 2ⁿ can only be 1.
        let bits = self.bits();
        let n64 = u64::from(n);
        if bits <= n64 {
            return BigUint::one();
        }

        // If we fit in `u64`, compute the root that way.
        if let Some(x) = self.to_u64() {
            return x.nth_root(n).into();
        }

        let max_bits = bits / n64 + 1;

        #[cfg(feature = "std")]
        let guess = if let Some(f) = self.to_f64() {
            // We fit in `f64` (lossy), so get a better initial guess from that.
            BigUint::from_f64((f.ln() / f64::from(n)).exp()).unwrap()
        } else {
            // Try to guess by scaling down such that it does fit in `f64`.
            // With some (x * 2ⁿᵏ), its nth root ≈ (ⁿ√x * 2ᵏ)
            let extra_bits = bits - (f64::MAX_EXP as u64 - 1);
            let root_scale = extra_bits.div_ceil(&n64);
            let scale = root_scale * n64;
            if scale < bits && bits - scale > n64 {
                (self >> scale).nth_root(n) << root_scale
            } else {
                BigUint::one() << max_bits
            }
        };

        #[cfg(not(feature = "std"))]
        let guess = BigUint::one() << max_bits;

        let n_min_1 = n - 1;
        fixpoint(guess, max_bits, move |s| {
            let q = self / s.pow(n_min_1);
            let t = n_min_1 * s + q;
            t / n
        })
    }

    // Reference:
    // Brent & Zimmermann, Modern Computer Arithmetic, v0.5.9, Algorithm 1.13
    fn sqrt(&self) -> Self {
        if self.is_zero() || self.is_one() {
            return self.clone();
        }

        // If we fit in `u64`, compute the root that way.
        if let Some(x) = self.to_u64() {
            return x.sqrt().into();
        }

        let bits = self.bits();
        let max_bits = bits / 2 + 1;

        #[cfg(feature = "std")]
        let guess = if let Some(f) = self.to_f64() {
            // We fit in `f64` (lossy), so get a better initial guess from that.
            BigUint::from_f64(f.sqrt()).unwrap()
        } else {
            // Try to guess by scaling down such that it does fit in `f64`.
            // With some (x * 2²ᵏ), its sqrt ≈ (√x * 2ᵏ)
            let extra_bits = bits - (f64::MAX_EXP as u64 - 1);
            let root_scale = (extra_bits + 1) / 2;
            let scale = root_scale * 2;
            (self >> scale).sqrt() << root_scale
        };

        #[cfg(not(feature = "std"))]
        let guess = BigUint::one() << max_bits;

        fixpoint(guess, max_bits, move |s| {
            let q = self / s;
            let t = s + q;
            t >> 1
        })
    }

    fn cbrt(&self) -> Self {
        if self.is_zero() || self.is_one() {
            return self.clone();
        }

        // If we fit in `u64`, compute the root that way.
        if let Some(x) = self.to_u64() {
            return x.cbrt().into();
        }

        let bits = self.bits();
        let max_bits = bits / 3 + 1;

        #[cfg(feature = "std")]
        let guess = if let Some(f) = self.to_f64() {
            // We fit in `f64` (lossy), so get a better initial guess from that.
            BigUint::from_f64(f.cbrt()).unwrap()
        } else {
            // Try to guess by scaling down such that it does fit in `f64`.
            // With some (x * 2³ᵏ), its cbrt ≈ (∛x * 2ᵏ)
            let extra_bits = bits - (f64::MAX_EXP as u64 - 1);
            let root_scale = (extra_bits + 2) / 3;
            let scale = root_scale * 3;
            (self >> scale).cbrt() << root_scale
        };

        #[cfg(not(feature = "std"))]
        let guess = BigUint::one() << max_bits;

        fixpoint(guess, max_bits, move |s| {
            let q = self / (s * s);
            let t = (s << 1) + q;
            t / 3u32
        })
    }
}

fn high_bits_to_u64(v: &BigUint) -> u64 {
    match v.data.len() {
        0 => 0,
        1 => u64::from(v.data[0]),
        _ => {
            let mut bits = v.bits();
            let mut ret = 0u64;
            let mut ret_bits = 0;

            for d in v.data.iter().rev() {
                let digit_bits = (bits - 1) % u64::from(big_digit::BITS) + 1;
                let bits_want = cmp::min(64 - ret_bits, digit_bits);

                if bits_want != 64 {
                    ret <<= bits_want;
                }
                ret |= u64::from(*d) >> (digit_bits - bits_want);
                ret_bits += bits_want;
                bits -= bits_want;

                if ret_bits == 64 {
                    break;
                }
            }

            ret
        }
    }
}

impl ToPrimitive for BigUint {
    #[inline]
    fn to_i64(&self) -> Option<i64> {
        self.to_u64().as_ref().and_then(u64::to_i64)
    }

    #[inline]
    fn to_i128(&self) -> Option<i128> {
        self.to_u128().as_ref().and_then(u128::to_i128)
    }

    #[inline]
    fn to_u64(&self) -> Option<u64> {
        let mut ret: u64 = 0;
        let mut bits = 0;

        for i in self.data.iter() {
            if bits >= 64 {
                return None;
            }

            ret += u64::from(*i) << bits;
            bits += big_digit::BITS;
        }

        Some(ret)
    }

    #[inline]
    fn to_u128(&self) -> Option<u128> {
        let mut ret: u128 = 0;
        let mut bits = 0;

        for i in self.data.iter() {
            if bits >= 128 {
                return None;
            }

            ret |= u128::from(*i) << bits;
            bits += big_digit::BITS;
        }

        Some(ret)
    }

    #[inline]
    fn to_f32(&self) -> Option<f32> {
        let mantissa = high_bits_to_u64(self);
        let exponent = self.bits() - u64::from(fls(mantissa));

        if exponent > f32::MAX_EXP as u64 {
            None
        } else {
            let ret = (mantissa as f32) * 2.0f32.powi(exponent as i32);
            if ret.is_infinite() {
                None
            } else {
                Some(ret)
            }
        }
    }

    #[inline]
    fn to_f64(&self) -> Option<f64> {
        let mantissa = high_bits_to_u64(self);
        let exponent = self.bits() - u64::from(fls(mantissa));

        if exponent > f64::MAX_EXP as u64 {
            None
        } else {
            let ret = (mantissa as f64) * 2.0f64.powi(exponent as i32);
            if ret.is_infinite() {
                None
            } else {
                Some(ret)
            }
        }
    }
}

macro_rules! impl_try_from_biguint {
    ($T:ty, $to_ty:path) => {
        #[cfg(has_try_from)]
        impl TryFrom<&BigUint> for $T {
            type Error = TryFromBigIntError<()>;

            #[inline]
            fn try_from(value: &BigUint) -> Result<$T, TryFromBigIntError<()>> {
                $to_ty(value).ok_or(TryFromBigIntError::new(()))
            }
        }

        #[cfg(has_try_from)]
        impl TryFrom<BigUint> for $T {
            type Error = TryFromBigIntError<BigUint>;

            #[inline]
            fn try_from(value: BigUint) -> Result<$T, TryFromBigIntError<BigUint>> {
                <$T>::try_from(&value).map_err(|_| TryFromBigIntError::new(value))
            }
        }
    };
}

impl_try_from_biguint!(u8, ToPrimitive::to_u8);
impl_try_from_biguint!(u16, ToPrimitive::to_u16);
impl_try_from_biguint!(u32, ToPrimitive::to_u32);
impl_try_from_biguint!(u64, ToPrimitive::to_u64);
impl_try_from_biguint!(usize, ToPrimitive::to_usize);
impl_try_from_biguint!(u128, ToPrimitive::to_u128);

impl_try_from_biguint!(i8, ToPrimitive::to_i8);
impl_try_from_biguint!(i16, ToPrimitive::to_i16);
impl_try_from_biguint!(i32, ToPrimitive::to_i32);
impl_try_from_biguint!(i64, ToPrimitive::to_i64);
impl_try_from_biguint!(isize, ToPrimitive::to_isize);
impl_try_from_biguint!(i128, ToPrimitive::to_i128);

impl FromPrimitive for BigUint {
    #[inline]
    fn from_i64(n: i64) -> Option<BigUint> {
        if n >= 0 {
            Some(BigUint::from(n as u64))
        } else {
            None
        }
    }

    #[inline]
    fn from_i128(n: i128) -> Option<BigUint> {
        if n >= 0 {
            Some(BigUint::from(n as u128))
        } else {
            None
        }
    }

    #[inline]
    fn from_u64(n: u64) -> Option<BigUint> {
        Some(BigUint::from(n))
    }

    #[inline]
    fn from_u128(n: u128) -> Option<BigUint> {
        Some(BigUint::from(n))
    }

    #[inline]
    fn from_f64(mut n: f64) -> Option<BigUint> {
        // handle NAN, INFINITY, NEG_INFINITY
        if !n.is_finite() {
            return None;
        }

        // match the rounding of casting from float to int
        n = n.trunc();

        // handle 0.x, -0.x
        if n.is_zero() {
            return Some(BigUint::zero());
        }

        let (mantissa, exponent, sign) = FloatCore::integer_decode(n);

        if sign == -1 {
            return None;
        }

        let mut ret = BigUint::from(mantissa);
        match exponent.cmp(&0) {
            Greater => ret <<= exponent as usize,
            Equal => {}
            Less => ret >>= (-exponent) as usize,
        }
        Some(ret)
    }
}

impl From<u64> for BigUint {
    #[inline]
    fn from(mut n: u64) -> Self {
        let mut ret: BigUint = Zero::zero();

        while n != 0 {
            ret.data.push(n as BigDigit);
            // don't overflow if BITS is 64:
            n = (n >> 1) >> (big_digit::BITS - 1);
        }

        ret
    }
}

impl From<u128> for BigUint {
    #[inline]
    fn from(mut n: u128) -> Self {
        let mut ret: BigUint = Zero::zero();

        while n != 0 {
            ret.data.push(n as BigDigit);
            n >>= big_digit::BITS;
        }

        ret
    }
}

macro_rules! impl_biguint_from_uint {
    ($T:ty) => {
        impl From<$T> for BigUint {
            #[inline]
            fn from(n: $T) -> Self {
                BigUint::from(n as u64)
            }
        }
    };
}

impl_biguint_from_uint!(u8);
impl_biguint_from_uint!(u16);
impl_biguint_from_uint!(u32);
impl_biguint_from_uint!(usize);

macro_rules! impl_biguint_try_from_int {
    ($T:ty, $from_ty:path) => {
        #[cfg(has_try_from)]
        impl TryFrom<$T> for BigUint {
            type Error = TryFromBigIntError<()>;

            #[inline]
            fn try_from(value: $T) -> Result<BigUint, TryFromBigIntError<()>> {
                $from_ty(value).ok_or(TryFromBigIntError::new(()))
            }
        }
    };
}

impl_biguint_try_from_int!(i8, FromPrimitive::from_i8);
impl_biguint_try_from_int!(i16, FromPrimitive::from_i16);
impl_biguint_try_from_int!(i32, FromPrimitive::from_i32);
impl_biguint_try_from_int!(i64, FromPrimitive::from_i64);
impl_biguint_try_from_int!(isize, FromPrimitive::from_isize);
impl_biguint_try_from_int!(i128, FromPrimitive::from_i128);

/// A generic trait for converting a value to a `BigUint`.
pub trait ToBigUint {
    /// Converts the value of `self` to a `BigUint`.
    fn to_biguint(&self) -> Option<BigUint>;
}

impl ToBigUint for BigUint {
    #[inline]
    fn to_biguint(&self) -> Option<BigUint> {
        Some(self.clone())
    }
}

macro_rules! impl_to_biguint {
    ($T:ty, $from_ty:path) => {
        impl ToBigUint for $T {
            #[inline]
            fn to_biguint(&self) -> Option<BigUint> {
                $from_ty(*self)
            }
        }
    };
}

impl_to_biguint!(isize, FromPrimitive::from_isize);
impl_to_biguint!(i8, FromPrimitive::from_i8);
impl_to_biguint!(i16, FromPrimitive::from_i16);
impl_to_biguint!(i32, FromPrimitive::from_i32);
impl_to_biguint!(i64, FromPrimitive::from_i64);
impl_to_biguint!(i128, FromPrimitive::from_i128);

impl_to_biguint!(usize, FromPrimitive::from_usize);
impl_to_biguint!(u8, FromPrimitive::from_u8);
impl_to_biguint!(u16, FromPrimitive::from_u16);
impl_to_biguint!(u32, FromPrimitive::from_u32);
impl_to_biguint!(u64, FromPrimitive::from_u64);
impl_to_biguint!(u128, FromPrimitive::from_u128);

impl_to_biguint!(f32, FromPrimitive::from_f32);
impl_to_biguint!(f64, FromPrimitive::from_f64);

// Extract bitwise digits that evenly divide BigDigit
fn to_bitwise_digits_le(u: &BigUint, bits: u8) -> Vec<u8> {
    debug_assert!(!u.is_zero() && bits <= 8 && big_digit::BITS % bits == 0);

    let last_i = u.data.len() - 1;
    let mask: BigDigit = (1 << bits) - 1;
    let digits_per_big_digit = big_digit::BITS / bits;
    let digits = u
        .bits()
        .div_ceil(&u64::from(bits))
        .to_usize()
        .unwrap_or(core::usize::MAX);
    let mut res = Vec::with_capacity(digits);

    for mut r in u.data[..last_i].iter().cloned() {
        for _ in 0..digits_per_big_digit {
            res.push((r & mask) as u8);
            r >>= bits;
        }
    }

    let mut r = u.data[last_i];
    while r != 0 {
        res.push((r & mask) as u8);
        r >>= bits;
    }

    res
}

// Extract bitwise digits that don't evenly divide BigDigit
fn to_inexact_bitwise_digits_le(u: &BigUint, bits: u8) -> Vec<u8> {
    debug_assert!(!u.is_zero() && bits <= 8 && big_digit::BITS % bits != 0);

    let mask: BigDigit = (1 << bits) - 1;
    let digits = u
        .bits()
        .div_ceil(&u64::from(bits))
        .to_usize()
        .unwrap_or(core::usize::MAX);
    let mut res = Vec::with_capacity(digits);

    let mut r = 0;
    let mut rbits = 0;

    for c in &u.data {
        r |= *c << rbits;
        rbits += big_digit::BITS;

        while rbits >= bits {
            res.push((r & mask) as u8);
            r >>= bits;

            // r had more bits than it could fit - grab the bits we lost
            if rbits > big_digit::BITS {
                r = *c >> (big_digit::BITS - (rbits - bits));
            }

            rbits -= bits;
        }
    }

    if rbits != 0 {
        res.push(r as u8);
    }

    while let Some(&0) = res.last() {
        res.pop();
    }

    res
}

// Extract little-endian radix digits
#[inline(always)] // forced inline to get const-prop for radix=10
fn to_radix_digits_le(u: &BigUint, radix: u32) -> Vec<u8> {
    debug_assert!(!u.is_zero() && !radix.is_power_of_two());

    #[cfg(feature = "std")]
    let radix_log2 = f64::from(radix).log2();
    #[cfg(not(feature = "std"))]
    let radix_log2 = ilog2(radix) as f64;

    // Estimate how big the result will be, so we can pre-allocate it.
    let radix_digits = ((u.bits() as f64) / radix_log2).ceil();
    let mut res = Vec::with_capacity(radix_digits.to_usize().unwrap_or(0));

    let mut digits = u.clone();

    let (base, power) = get_radix_base(radix, big_digit::HALF_BITS);
    let radix = radix as BigDigit;

    while digits.data.len() > 1 {
        let (q, mut r) = div_rem_digit(digits, base);
        for _ in 0..power {
            res.push((r % radix) as u8);
            r /= radix;
        }
        digits = q;
    }

    let mut r = digits.data[0];
    while r != 0 {
        res.push((r % radix) as u8);
        r /= radix;
    }

    res
}

pub(crate) fn to_radix_le(u: &BigUint, radix: u32) -> Vec<u8> {
    if u.is_zero() {
        vec![0]
    } else if radix.is_power_of_two() {
        // Powers of two can use bitwise masks and shifting instead of division
        let bits = ilog2(radix);
        if big_digit::BITS % bits == 0 {
            to_bitwise_digits_le(u, bits)
        } else {
            to_inexact_bitwise_digits_le(u, bits)
        }
    } else if radix == 10 {
        // 10 is so common that it's worth separating out for const-propagation.
        // Optimizers can often turn constant division into a faster multiplication.
        to_radix_digits_le(u, 10)
    } else {
        to_radix_digits_le(u, radix)
    }
}

pub(crate) fn to_str_radix_reversed(u: &BigUint, radix: u32) -> Vec<u8> {
    assert!(2 <= radix && radix <= 36, "The radix must be within 2...36");

    if u.is_zero() {
        return vec![b'0'];
    }

    let mut res = to_radix_le(u, radix);

    // Now convert everything to ASCII digits.
    for r in &mut res {
        debug_assert!(u32::from(*r) < radix);
        if *r < 10 {
            *r += b'0';
        } else {
            *r += b'a' - 10;
        }
    }
    res
}

/// Creates and initializes a `BigUint`.
///
/// The digits are in little-endian base matching `BigDigit`.
#[inline]
pub(crate) fn biguint_from_vec(digits: Vec<BigDigit>) -> BigUint {
    BigUint { data: digits }.normalized()
}

impl BigUint {
    /// Creates and initializes a `BigUint`.
    ///
    /// The base 2<sup>32</sup> digits are ordered least significant digit first.
    #[inline]
    pub fn new(digits: Vec<u32>) -> BigUint {
        let mut big = BigUint::zero();

        #[cfg(not(u64_digit))]
        {
            big.data = digits;
            big.normalize();
        }

        #[cfg(u64_digit)]
        big.assign_from_slice(&digits);

        big
    }

    /// Creates and initializes a `BigUint`.
    ///
    /// The base 2<sup>32</sup> digits are ordered least significant digit first.
    #[inline]
    pub fn from_slice(slice: &[u32]) -> BigUint {
        let mut big = BigUint::zero();
        big.assign_from_slice(slice);
        big
    }

    /// Assign a value to a `BigUint`.
    ///
    /// The base 2<sup>32</sup> digits are ordered least significant digit first.
    #[inline]
    pub fn assign_from_slice(&mut self, slice: &[u32]) {
        self.data.clear();

        #[cfg(not(u64_digit))]
        self.data.extend_from_slice(slice);

        #[cfg(u64_digit)]
        self.data.extend(slice.chunks(2).map(|chunk| {
            // raw could have odd length
            let mut digit = BigDigit::from(chunk[0]);
            if let Some(&hi) = chunk.get(1) {
                digit |= BigDigit::from(hi) << 32;
            }
            digit
        }));

        self.normalize();
    }

    /// Creates and initializes a `BigUint`.
    ///
    /// The bytes are in big-endian byte order.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::BigUint;
    ///
    /// assert_eq!(BigUint::from_bytes_be(b"A"),
    ///            BigUint::parse_bytes(b"65", 10).unwrap());
    /// assert_eq!(BigUint::from_bytes_be(b"AA"),
    ///            BigUint::parse_bytes(b"16705", 10).unwrap());
    /// assert_eq!(BigUint::from_bytes_be(b"AB"),
    ///            BigUint::parse_bytes(b"16706", 10).unwrap());
    /// assert_eq!(BigUint::from_bytes_be(b"Hello world!"),
    ///            BigUint::parse_bytes(b"22405534230753963835153736737", 10).unwrap());
    /// ```
    #[inline]
    pub fn from_bytes_be(bytes: &[u8]) -> BigUint {
        if bytes.is_empty() {
            Zero::zero()
        } else {
            let mut v = bytes.to_vec();
            v.reverse();
            BigUint::from_bytes_le(&*v)
        }
    }

    /// Creates and initializes a `BigUint`.
    ///
    /// The bytes are in little-endian byte order.
    #[inline]
    pub fn from_bytes_le(bytes: &[u8]) -> BigUint {
        if bytes.is_empty() {
            Zero::zero()
        } else {
            from_bitwise_digits_le(bytes, 8)
        }
    }

    /// Creates and initializes a `BigUint`. The input slice must contain
    /// ascii/utf8 characters in [0-9a-zA-Z].
    /// `radix` must be in the range `2...36`.
    ///
    /// The function `from_str_radix` from the `Num` trait provides the same logic
    /// for `&str` buffers.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{BigUint, ToBigUint};
    ///
    /// assert_eq!(BigUint::parse_bytes(b"1234", 10), ToBigUint::to_biguint(&1234));
    /// assert_eq!(BigUint::parse_bytes(b"ABCD", 16), ToBigUint::to_biguint(&0xABCD));
    /// assert_eq!(BigUint::parse_bytes(b"G", 16), None);
    /// ```
    #[inline]
    pub fn parse_bytes(buf: &[u8], radix: u32) -> Option<BigUint> {
        let s = str::from_utf8(buf).ok()?;
        BigUint::from_str_radix(s, radix).ok()
    }

    /// Creates and initializes a `BigUint`. Each u8 of the input slice is
    /// interpreted as one digit of the number
    /// and must therefore be less than `radix`.
    ///
    /// The bytes are in big-endian byte order.
    /// `radix` must be in the range `2...256`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{BigUint};
    ///
    /// let inbase190 = &[15, 33, 125, 12, 14];
    /// let a = BigUint::from_radix_be(inbase190, 190).unwrap();
    /// assert_eq!(a.to_radix_be(190), inbase190);
    /// ```
    pub fn from_radix_be(buf: &[u8], radix: u32) -> Option<BigUint> {
        assert!(
            2 <= radix && radix <= 256,
            "The radix must be within 2...256"
        );

        if radix != 256 && buf.iter().any(|&b| b >= radix as u8) {
            return None;
        }

        let res = if radix.is_power_of_two() {
            // Powers of two can use bitwise masks and shifting instead of multiplication
            let bits = ilog2(radix);
            let mut v = Vec::from(buf);
            v.reverse();
            if big_digit::BITS % bits == 0 {
                from_bitwise_digits_le(&v, bits)
            } else {
                from_inexact_bitwise_digits_le(&v, bits)
            }
        } else {
            from_radix_digits_be(buf, radix)
        };

        Some(res)
    }

    /// Creates and initializes a `BigUint`. Each u8 of the input slice is
    /// interpreted as one digit of the number
    /// and must therefore be less than `radix`.
    ///
    /// The bytes are in little-endian byte order.
    /// `radix` must be in the range `2...256`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::{BigUint};
    ///
    /// let inbase190 = &[14, 12, 125, 33, 15];
    /// let a = BigUint::from_radix_be(inbase190, 190).unwrap();
    /// assert_eq!(a.to_radix_be(190), inbase190);
    /// ```
    pub fn from_radix_le(buf: &[u8], radix: u32) -> Option<BigUint> {
        assert!(
            2 <= radix && radix <= 256,
            "The radix must be within 2...256"
        );

        if radix != 256 && buf.iter().any(|&b| b >= radix as u8) {
            return None;
        }

        let res = if radix.is_power_of_two() {
            // Powers of two can use bitwise masks and shifting instead of multiplication
            let bits = ilog2(radix);
            if big_digit::BITS % bits == 0 {
                from_bitwise_digits_le(buf, bits)
            } else {
                from_inexact_bitwise_digits_le(buf, bits)
            }
        } else {
            let mut v = Vec::from(buf);
            v.reverse();
            from_radix_digits_be(&v, radix)
        };

        Some(res)
    }

    /// Returns the byte representation of the `BigUint` in big-endian byte order.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::BigUint;
    ///
    /// let i = BigUint::parse_bytes(b"1125", 10).unwrap();
    /// assert_eq!(i.to_bytes_be(), vec![4, 101]);
    /// ```
    #[inline]
    pub fn to_bytes_be(&self) -> Vec<u8> {
        let mut v = self.to_bytes_le();
        v.reverse();
        v
    }

    /// Returns the byte representation of the `BigUint` in little-endian byte order.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::BigUint;
    ///
    /// let i = BigUint::parse_bytes(b"1125", 10).unwrap();
    /// assert_eq!(i.to_bytes_le(), vec![101, 4]);
    /// ```
    #[inline]
    pub fn to_bytes_le(&self) -> Vec<u8> {
        if self.is_zero() {
            vec![0]
        } else {
            to_bitwise_digits_le(self, 8)
        }
    }

    /// Returns the `u32` digits representation of the `BigUint` ordered least significant digit
    /// first.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::BigUint;
    ///
    /// assert_eq!(BigUint::from(1125u32).to_u32_digits(), vec![1125]);
    /// assert_eq!(BigUint::from(4294967295u32).to_u32_digits(), vec![4294967295]);
    /// assert_eq!(BigUint::from(4294967296u64).to_u32_digits(), vec![0, 1]);
    /// assert_eq!(BigUint::from(112500000000u64).to_u32_digits(), vec![830850304, 26]);
    /// ```
    #[inline]
    pub fn to_u32_digits(&self) -> Vec<u32> {
        let mut digits = Vec::new();

        #[cfg(not(u64_digit))]
        digits.clone_from(&self.data);

        #[cfg(u64_digit)]
        {
            if let Some((&last, data)) = self.data.split_last() {
                let last_lo = last as u32;
                let last_hi = (last >> 32) as u32;
                let u32_len = data.len() * 2 + 1 + (last_hi != 0) as usize;
                digits.reserve_exact(u32_len);
                for &x in data {
                    digits.push(x as u32);
                    digits.push((x >> 32) as u32);
                }
                digits.push(last_lo);
                if last_hi != 0 {
                    digits.push(last_hi);
                }
            }
        }

        digits
    }

    /// Returns the integer formatted as a string in the given radix.
    /// `radix` must be in the range `2...36`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::BigUint;
    ///
    /// let i = BigUint::parse_bytes(b"ff", 16).unwrap();
    /// assert_eq!(i.to_str_radix(16), "ff");
    /// ```
    #[inline]
    pub fn to_str_radix(&self, radix: u32) -> String {
        let mut v = to_str_radix_reversed(self, radix);
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
    /// use num_bigint::BigUint;
    ///
    /// assert_eq!(BigUint::from(0xFFFFu64).to_radix_be(159),
    ///            vec![2, 94, 27]);
    /// // 0xFFFF = 65535 = 2*(159^2) + 94*159 + 27
    /// ```
    #[inline]
    pub fn to_radix_be(&self, radix: u32) -> Vec<u8> {
        let mut v = to_radix_le(self, radix);
        v.reverse();
        v
    }

    /// Returns the integer in the requested base in little-endian digit order.
    /// The output is not given in a human readable alphabet but as a zero
    /// based u8 number.
    /// `radix` must be in the range `2...256`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_bigint::BigUint;
    ///
    /// assert_eq!(BigUint::from(0xFFFFu64).to_radix_le(159),
    ///            vec![27, 94, 2]);
    /// // 0xFFFF = 65535 = 27 + 94*159 + 2*(159^2)
    /// ```
    #[inline]
    pub fn to_radix_le(&self, radix: u32) -> Vec<u8> {
        to_radix_le(self, radix)
    }

    /// Determines the fewest bits necessary to express the `BigUint`.
    #[inline]
    pub fn bits(&self) -> u64 {
        if self.is_zero() {
            return 0;
        }
        let zeros: u64 = self.data.last().unwrap().leading_zeros().into();
        self.data.len() as u64 * u64::from(big_digit::BITS) - zeros
    }

    /// Strips off trailing zero bigdigits - comparisons require the last element in the vector to
    /// be nonzero.
    #[inline]
    fn normalize(&mut self) {
        while let Some(&0) = self.data.last() {
            self.data.pop();
        }
    }

    /// Returns a normalized `BigUint`.
    #[inline]
    fn normalized(mut self) -> BigUint {
        self.normalize();
        self
    }

    /// Returns `self ^ exponent`.
    pub fn pow(&self, exponent: u32) -> Self {
        Pow::pow(self, exponent)
    }

    /// Returns `(self ^ exponent) % modulus`.
    ///
    /// Panics if the modulus is zero.
    pub fn modpow(&self, exponent: &Self, modulus: &Self) -> Self {
        assert!(
            !modulus.is_zero(),
            "attempt to calculate with zero modulus!"
        );

        if modulus.is_odd() {
            // For an odd modulus, we can use Montgomery multiplication in base 2^32.
            monty_modpow(self, exponent, modulus)
        } else {
            // Otherwise do basically the same as `num::pow`, but with a modulus.
            plain_modpow(self, &exponent.data, modulus)
        }
    }

    /// Returns the truncated principal square root of `self` --
    /// see [Roots::sqrt](https://docs.rs/num-integer/0.1/num_integer/trait.Roots.html#method.sqrt)
    pub fn sqrt(&self) -> Self {
        Roots::sqrt(self)
    }

    /// Returns the truncated principal cube root of `self` --
    /// see [Roots::cbrt](https://docs.rs/num-integer/0.1/num_integer/trait.Roots.html#method.cbrt).
    pub fn cbrt(&self) -> Self {
        Roots::cbrt(self)
    }

    /// Returns the truncated principal `n`th root of `self` --
    /// see [Roots::nth_root](https://docs.rs/num-integer/0.1/num_integer/trait.Roots.html#tymethod.nth_root).
    pub fn nth_root(&self, n: u32) -> Self {
        Roots::nth_root(self, n)
    }

    /// Returns the number of least-significant bits that are zero,
    /// or `None` if the entire number is zero.
    pub fn trailing_zeros(&self) -> Option<u64> {
        let i = self.data.iter().position(|&digit| digit != 0)?;
        let zeros: u64 = self.data[i].trailing_zeros().into();
        Some(i as u64 * u64::from(big_digit::BITS) + zeros)
    }
}

fn plain_modpow(base: &BigUint, exp_data: &[BigDigit], modulus: &BigUint) -> BigUint {
    assert!(
        !modulus.is_zero(),
        "attempt to calculate with zero modulus!"
    );

    let i = match exp_data.iter().position(|&r| r != 0) {
        None => return BigUint::one(),
        Some(i) => i,
    };

    let mut base = base % modulus;
    for _ in 0..i {
        for _ in 0..big_digit::BITS {
            base = &base * &base % modulus;
        }
    }

    let mut r = exp_data[i];
    let mut b = 0u8;
    while r.is_even() {
        base = &base * &base % modulus;
        r >>= 1;
        b += 1;
    }

    let mut exp_iter = exp_data[i + 1..].iter();
    if exp_iter.len() == 0 && r.is_one() {
        return base;
    }

    let mut acc = base.clone();
    r >>= 1;
    b += 1;

    {
        let mut unit = |exp_is_odd| {
            base = &base * &base % modulus;
            if exp_is_odd {
                acc = &acc * &base % modulus;
            }
        };

        if let Some(&last) = exp_iter.next_back() {
            // consume exp_data[i]
            for _ in b..big_digit::BITS {
                unit(r.is_odd());
                r >>= 1;
            }

            // consume all other digits before the last
            for &r in exp_iter {
                let mut r = r;
                for _ in 0..big_digit::BITS {
                    unit(r.is_odd());
                    r >>= 1;
                }
            }
            r = last;
        }

        debug_assert_ne!(r, 0);
        while !r.is_zero() {
            unit(r.is_odd());
            r >>= 1;
        }
    }
    acc
}

#[test]
fn test_plain_modpow() {
    let two = &BigUint::from(2u32);
    let modulus = BigUint::from(0x1100u32);

    let exp = vec![0, 0b1];
    assert_eq!(
        two.pow(0b1_00000000_u32) % &modulus,
        plain_modpow(&two, &exp, &modulus)
    );
    let exp = vec![0, 0b10];
    assert_eq!(
        two.pow(0b10_00000000_u32) % &modulus,
        plain_modpow(&two, &exp, &modulus)
    );
    let exp = vec![0, 0b110010];
    assert_eq!(
        two.pow(0b110010_00000000_u32) % &modulus,
        plain_modpow(&two, &exp, &modulus)
    );
    let exp = vec![0b1, 0b1];
    assert_eq!(
        two.pow(0b1_00000001_u32) % &modulus,
        plain_modpow(&two, &exp, &modulus)
    );
    let exp = vec![0b1100, 0, 0b1];
    assert_eq!(
        two.pow(0b1_00000000_00001100_u32) % &modulus,
        plain_modpow(&two, &exp, &modulus)
    );
}

impl_sum_iter_type!(BigUint);
impl_product_iter_type!(BigUint);

pub(crate) trait IntDigits {
    fn digits(&self) -> &[BigDigit];
    fn digits_mut(&mut self) -> &mut Vec<BigDigit>;
    fn normalize(&mut self);
    fn capacity(&self) -> usize;
    fn len(&self) -> usize;
}

impl IntDigits for BigUint {
    #[inline]
    fn digits(&self) -> &[BigDigit] {
        &self.data
    }
    #[inline]
    fn digits_mut(&mut self) -> &mut Vec<BigDigit> {
        &mut self.data
    }
    #[inline]
    fn normalize(&mut self) {
        self.normalize();
    }
    #[inline]
    fn capacity(&self) -> usize {
        self.data.capacity()
    }
    #[inline]
    fn len(&self) -> usize {
        self.data.len()
    }
}

/// Combine four `u32`s into a single `u128`.
#[cfg(any(test, not(u64_digit)))]
#[inline]
fn u32_to_u128(a: u32, b: u32, c: u32, d: u32) -> u128 {
    u128::from(d) | (u128::from(c) << 32) | (u128::from(b) << 64) | (u128::from(a) << 96)
}

/// Split a single `u128` into four `u32`.
#[cfg(any(test, not(u64_digit)))]
#[inline]
fn u32_from_u128(n: u128) -> (u32, u32, u32, u32) {
    (
        (n >> 96) as u32,
        (n >> 64) as u32,
        (n >> 32) as u32,
        n as u32,
    )
}

#[cfg(feature = "serde")]
impl serde::Serialize for BigUint {
    #[cfg(not(u64_digit))]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        // Note: do not change the serialization format, or it may break forward
        // and backward compatibility of serialized data!  If we ever change the
        // internal representation, we should still serialize in base-`u32`.
        let data: &[u32] = &self.data;
        data.serialize(serializer)
    }

    #[cfg(u64_digit)]
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        use serde::ser::SerializeSeq;
        if let Some((&last, data)) = self.data.split_last() {
            let last_lo = last as u32;
            let last_hi = (last >> 32) as u32;
            let u32_len = data.len() * 2 + 1 + (last_hi != 0) as usize;
            let mut seq = serializer.serialize_seq(Some(u32_len))?;
            for &x in data {
                seq.serialize_element(&(x as u32))?;
                seq.serialize_element(&((x >> 32) as u32))?;
            }
            seq.serialize_element(&last_lo)?;
            if last_hi != 0 {
                seq.serialize_element(&last_hi)?;
            }
            seq.end()
        } else {
            let data: &[u32] = &[];
            data.serialize(serializer)
        }
    }
}

#[cfg(feature = "serde")]
impl<'de> serde::Deserialize<'de> for BigUint {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: serde::Deserializer<'de>,
    {
        use serde::de::{SeqAccess, Visitor};

        struct U32Visitor;

        impl<'de> Visitor<'de> for U32Visitor {
            type Value = BigUint;

            fn expecting(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
                formatter.write_str("a sequence of unsigned 32-bit numbers")
            }

            #[cfg(not(u64_digit))]
            fn visit_seq<S>(self, mut seq: S) -> Result<Self::Value, S::Error>
            where
                S: SeqAccess<'de>,
            {
                let len = seq.size_hint().unwrap_or(0);
                let mut data = Vec::with_capacity(len);

                while let Some(value) = seq.next_element::<u32>()? {
                    data.push(value);
                }

                Ok(biguint_from_vec(data))
            }

            #[cfg(u64_digit)]
            fn visit_seq<S>(self, mut seq: S) -> Result<Self::Value, S::Error>
            where
                S: SeqAccess<'de>,
            {
                let u32_len = seq.size_hint().unwrap_or(0);
                let len = u32_len.div_ceil(&2);
                let mut data = Vec::with_capacity(len);

                while let Some(lo) = seq.next_element::<u32>()? {
                    let mut value = BigDigit::from(lo);
                    if let Some(hi) = seq.next_element::<u32>()? {
                        value |= BigDigit::from(hi) << 32;
                        data.push(value);
                    } else {
                        data.push(value);
                        break;
                    }
                }

                Ok(biguint_from_vec(data))
            }
        }

        deserializer.deserialize_seq(U32Visitor)
    }
}

/// Returns the greatest power of the radix for the given bit size
#[inline]
fn get_radix_base(radix: u32, bits: u8) -> (BigDigit, usize) {
    mod gen {
        include! { concat!(env!("OUT_DIR"), "/radix_bases.rs") }
    }

    debug_assert!(
        2 <= radix && radix <= 256,
        "The radix must be within 2...256"
    );
    debug_assert!(!radix.is_power_of_two());
    debug_assert!(bits <= big_digit::BITS);

    match bits {
        16 => {
            let (base, power) = gen::BASES_16[radix as usize];
            (base as BigDigit, power)
        }
        32 => {
            let (base, power) = gen::BASES_32[radix as usize];
            (base as BigDigit, power)
        }
        64 => {
            let (base, power) = gen::BASES_64[radix as usize];
            (base as BigDigit, power)
        }
        _ => panic!("Invalid bigdigit size"),
    }
}

#[cfg(not(u64_digit))]
#[test]
fn test_from_slice() {
    fn check(slice: &[u32], data: &[BigDigit]) {
        assert_eq!(BigUint::from_slice(slice).data, data);
    }
    check(&[1], &[1]);
    check(&[0, 0, 0], &[]);
    check(&[1, 2, 0, 0], &[1, 2]);
    check(&[0, 0, 1, 2], &[0, 0, 1, 2]);
    check(&[0, 0, 1, 2, 0, 0], &[0, 0, 1, 2]);
    check(&[-1i32 as u32], &[-1i32 as BigDigit]);
}

#[cfg(u64_digit)]
#[test]
fn test_from_slice() {
    fn check(slice: &[u32], data: &[BigDigit]) {
        assert_eq!(
            BigUint::from_slice(slice).data,
            data,
            "from {:?}, to {:?}",
            slice,
            data
        );
    }
    check(&[1], &[1]);
    check(&[0, 0, 0], &[]);
    check(&[1, 2], &[8_589_934_593]);
    check(&[1, 2, 0, 0], &[8_589_934_593]);
    check(&[0, 0, 1, 2], &[0, 8_589_934_593]);
    check(&[0, 0, 1, 2, 0, 0], &[0, 8_589_934_593]);
    check(&[-1i32 as u32], &[(-1i32 as u32) as BigDigit]);
}

#[test]
fn test_u32_u128() {
    assert_eq!(u32_from_u128(0u128), (0, 0, 0, 0));
    assert_eq!(
        u32_from_u128(u128::max_value()),
        (
            u32::max_value(),
            u32::max_value(),
            u32::max_value(),
            u32::max_value()
        )
    );

    assert_eq!(
        u32_from_u128(u32::max_value() as u128),
        (0, 0, 0, u32::max_value())
    );

    assert_eq!(
        u32_from_u128(u64::max_value() as u128),
        (0, 0, u32::max_value(), u32::max_value())
    );

    assert_eq!(
        u32_from_u128((u64::max_value() as u128) + u32::max_value() as u128),
        (0, 1, 0, u32::max_value() - 1)
    );

    assert_eq!(u32_from_u128(36_893_488_151_714_070_528), (0, 2, 1, 0));
}

#[test]
fn test_u128_u32_roundtrip() {
    // roundtrips
    let values = vec![
        0u128,
        1u128,
        u64::max_value() as u128 * 3,
        u32::max_value() as u128,
        u64::max_value() as u128,
        (u64::max_value() as u128) + u32::max_value() as u128,
        u128::max_value(),
    ];

    for val in &values {
        let (a, b, c, d) = u32_from_u128(*val);
        assert_eq!(u32_to_u128(a, b, c, d), *val);
    }
}

#[test]
fn test_pow_biguint() {
    let base = BigUint::from(5u8);
    let exponent = BigUint::from(3u8);

    assert_eq!(BigUint::from(125u8), base.pow(exponent));
}
