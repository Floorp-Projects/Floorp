use std::ops::{Not, BitAnd, BitOr, BitXor, Shl, Shr};

use {Num, NumCast};
use bounds::Bounded;
use ops::checked::*;
use ops::saturating::Saturating;

pub trait PrimInt
    : Sized
    + Copy
    + Num + NumCast
    + Bounded
    + PartialOrd + Ord + Eq
    + Not<Output=Self>
    + BitAnd<Output=Self>
    + BitOr<Output=Self>
    + BitXor<Output=Self>
    + Shl<usize, Output=Self>
    + Shr<usize, Output=Self>
    + CheckedAdd<Output=Self>
    + CheckedSub<Output=Self>
    + CheckedMul<Output=Self>
    + CheckedDiv<Output=Self>
    + Saturating
{
    /// Returns the number of ones in the binary representation of `self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0b01001100u8;
    ///
    /// assert_eq!(n.count_ones(), 3);
    /// ```
    fn count_ones(self) -> u32;

    /// Returns the number of zeros in the binary representation of `self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0b01001100u8;
    ///
    /// assert_eq!(n.count_zeros(), 5);
    /// ```
    fn count_zeros(self) -> u32;

    /// Returns the number of leading zeros in the binary representation
    /// of `self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0b0101000u16;
    ///
    /// assert_eq!(n.leading_zeros(), 10);
    /// ```
    fn leading_zeros(self) -> u32;

    /// Returns the number of trailing zeros in the binary representation
    /// of `self`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0b0101000u16;
    ///
    /// assert_eq!(n.trailing_zeros(), 3);
    /// ```
    fn trailing_zeros(self) -> u32;

    /// Shifts the bits to the left by a specified amount amount, `n`, wrapping
    /// the truncated bits to the end of the resulting integer.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0x0123456789ABCDEFu64;
    /// let m = 0x3456789ABCDEF012u64;
    ///
    /// assert_eq!(n.rotate_left(12), m);
    /// ```
    fn rotate_left(self, n: u32) -> Self;

    /// Shifts the bits to the right by a specified amount amount, `n`, wrapping
    /// the truncated bits to the beginning of the resulting integer.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0x0123456789ABCDEFu64;
    /// let m = 0xDEF0123456789ABCu64;
    ///
    /// assert_eq!(n.rotate_right(12), m);
    /// ```
    fn rotate_right(self, n: u32) -> Self;

    /// Shifts the bits to the left by a specified amount amount, `n`, filling
    /// zeros in the least significant bits.
    ///
    /// This is bitwise equivalent to signed `Shl`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0x0123456789ABCDEFu64;
    /// let m = 0x3456789ABCDEF000u64;
    ///
    /// assert_eq!(n.signed_shl(12), m);
    /// ```
    fn signed_shl(self, n: u32) -> Self;

    /// Shifts the bits to the right by a specified amount amount, `n`, copying
    /// the "sign bit" in the most significant bits even for unsigned types.
    ///
    /// This is bitwise equivalent to signed `Shr`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0xFEDCBA9876543210u64;
    /// let m = 0xFFFFEDCBA9876543u64;
    ///
    /// assert_eq!(n.signed_shr(12), m);
    /// ```
    fn signed_shr(self, n: u32) -> Self;

    /// Shifts the bits to the left by a specified amount amount, `n`, filling
    /// zeros in the least significant bits.
    ///
    /// This is bitwise equivalent to unsigned `Shl`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0x0123456789ABCDEFi64;
    /// let m = 0x3456789ABCDEF000i64;
    ///
    /// assert_eq!(n.unsigned_shl(12), m);
    /// ```
    fn unsigned_shl(self, n: u32) -> Self;

    /// Shifts the bits to the right by a specified amount amount, `n`, filling
    /// zeros in the most significant bits.
    ///
    /// This is bitwise equivalent to unsigned `Shr`.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0xFEDCBA9876543210i64;
    /// let m = 0x000FEDCBA9876543i64;
    ///
    /// assert_eq!(n.unsigned_shr(12), m);
    /// ```
    fn unsigned_shr(self, n: u32) -> Self;

    /// Reverses the byte order of the integer.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0x0123456789ABCDEFu64;
    /// let m = 0xEFCDAB8967452301u64;
    ///
    /// assert_eq!(n.swap_bytes(), m);
    /// ```
    fn swap_bytes(self) -> Self;

    /// Convert an integer from big endian to the target's endianness.
    ///
    /// On big endian this is a no-op. On little endian the bytes are swapped.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0x0123456789ABCDEFu64;
    ///
    /// if cfg!(target_endian = "big") {
    ///     assert_eq!(u64::from_be(n), n)
    /// } else {
    ///     assert_eq!(u64::from_be(n), n.swap_bytes())
    /// }
    /// ```
    fn from_be(x: Self) -> Self;

    /// Convert an integer from little endian to the target's endianness.
    ///
    /// On little endian this is a no-op. On big endian the bytes are swapped.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0x0123456789ABCDEFu64;
    ///
    /// if cfg!(target_endian = "little") {
    ///     assert_eq!(u64::from_le(n), n)
    /// } else {
    ///     assert_eq!(u64::from_le(n), n.swap_bytes())
    /// }
    /// ```
    fn from_le(x: Self) -> Self;

    /// Convert `self` to big endian from the target's endianness.
    ///
    /// On big endian this is a no-op. On little endian the bytes are swapped.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0x0123456789ABCDEFu64;
    ///
    /// if cfg!(target_endian = "big") {
    ///     assert_eq!(n.to_be(), n)
    /// } else {
    ///     assert_eq!(n.to_be(), n.swap_bytes())
    /// }
    /// ```
    fn to_be(self) -> Self;

    /// Convert `self` to little endian from the target's endianness.
    ///
    /// On little endian this is a no-op. On big endian the bytes are swapped.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// let n = 0x0123456789ABCDEFu64;
    ///
    /// if cfg!(target_endian = "little") {
    ///     assert_eq!(n.to_le(), n)
    /// } else {
    ///     assert_eq!(n.to_le(), n.swap_bytes())
    /// }
    /// ```
    fn to_le(self) -> Self;

    /// Raises self to the power of `exp`, using exponentiation by squaring.
    ///
    /// # Examples
    ///
    /// ```
    /// use num_traits::PrimInt;
    ///
    /// assert_eq!(2i32.pow(4), 16);
    /// ```
    fn pow(self, mut exp: u32) -> Self;
}

macro_rules! prim_int_impl {
    ($T:ty, $S:ty, $U:ty) => (
        impl PrimInt for $T {
            #[inline]
            fn count_ones(self) -> u32 {
                <$T>::count_ones(self)
            }

            #[inline]
            fn count_zeros(self) -> u32 {
                <$T>::count_zeros(self)
            }

            #[inline]
            fn leading_zeros(self) -> u32 {
                <$T>::leading_zeros(self)
            }

            #[inline]
            fn trailing_zeros(self) -> u32 {
                <$T>::trailing_zeros(self)
            }

            #[inline]
            fn rotate_left(self, n: u32) -> Self {
                <$T>::rotate_left(self, n)
            }

            #[inline]
            fn rotate_right(self, n: u32) -> Self {
                <$T>::rotate_right(self, n)
            }

            #[inline]
            fn signed_shl(self, n: u32) -> Self {
                ((self as $S) << n) as $T
            }

            #[inline]
            fn signed_shr(self, n: u32) -> Self {
                ((self as $S) >> n) as $T
            }

            #[inline]
            fn unsigned_shl(self, n: u32) -> Self {
                ((self as $U) << n) as $T
            }

            #[inline]
            fn unsigned_shr(self, n: u32) -> Self {
                ((self as $U) >> n) as $T
            }

            #[inline]
            fn swap_bytes(self) -> Self {
                <$T>::swap_bytes(self)
            }

            #[inline]
            fn from_be(x: Self) -> Self {
                <$T>::from_be(x)
            }

            #[inline]
            fn from_le(x: Self) -> Self {
                <$T>::from_le(x)
            }

            #[inline]
            fn to_be(self) -> Self {
                <$T>::to_be(self)
            }

            #[inline]
            fn to_le(self) -> Self {
                <$T>::to_le(self)
            }

            #[inline]
            fn pow(self, exp: u32) -> Self {
                <$T>::pow(self, exp)
            }
        }
    )
}

// prim_int_impl!(type, signed, unsigned);
prim_int_impl!(u8,    i8,    u8);
prim_int_impl!(u16,   i16,   u16);
prim_int_impl!(u32,   i32,   u32);
prim_int_impl!(u64,   i64,   u64);
prim_int_impl!(usize, isize, usize);
prim_int_impl!(i8,    i8,    u8);
prim_int_impl!(i16,   i16,   u16);
prim_int_impl!(i32,   i32,   u32);
prim_int_impl!(i64,   i64,   u64);
prim_int_impl!(isize, isize, usize);
