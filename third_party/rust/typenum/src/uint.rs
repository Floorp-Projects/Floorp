//! Type-level unsigned integers.
//!
//!
//! **Type operators** implemented:
//!
//! From `core::ops`: `BitAnd`, `BitOr`, `BitXor`, `Shl`, `Shr`, `Add`, `Sub`,
//!                 `Mul`, `Div`, and `Rem`.
//! From `typenum`: `Same`, `Cmp`, and `Pow`.
//!
//! Rather than directly using the structs defined in this module, it is recommended that
//! you import and use the relevant aliases from the [consts](../consts/index.html) module.
//!
//! # Example
//! ```rust
//! use std::ops::{BitAnd, BitOr, BitXor, Shl, Shr, Add, Sub, Mul, Div, Rem};
//! use typenum::{Unsigned, U1, U2, U3, U4};
//!
//! assert_eq!(<U3 as BitAnd<U2>>::Output::to_u32(), 2);
//! assert_eq!(<U3 as BitOr<U4>>::Output::to_u32(), 7);
//! assert_eq!(<U3 as BitXor<U2>>::Output::to_u32(), 1);
//! assert_eq!(<U3 as Shl<U1>>::Output::to_u32(), 6);
//! assert_eq!(<U3 as Shr<U1>>::Output::to_u32(), 1);
//! assert_eq!(<U3 as Add<U2>>::Output::to_u32(), 5);
//! assert_eq!(<U3 as Sub<U2>>::Output::to_u32(), 1);
//! assert_eq!(<U3 as Mul<U2>>::Output::to_u32(), 6);
//! assert_eq!(<U3 as Div<U2>>::Output::to_u32(), 1);
//! assert_eq!(<U3 as Rem<U2>>::Output::to_u32(), 1);
//! ```
//!

use core::ops::{Add, BitAnd, BitOr, BitXor, Mul, Shl, Shr, Sub};
use core::marker::PhantomData;
use {Cmp, Equal, Greater, Len, Less, NonZero, Ord, Pow};

use bit::{B0, B1, Bit};

use private::{BitDiff, PrivateAnd, PrivateCmp, PrivatePow, PrivateSub, PrivateXor, Trim};

use private::{BitDiffOut, PrivateAndOut, PrivateCmpOut, PrivatePowOut, PrivateSubOut,
              PrivateXorOut, TrimOut};

use consts::{U0, U1};
use {Add1, Length, Or, Prod, Shleft, Shright, Square, Sub1, Sum};

pub use marker_traits::{PowerOfTwo, Unsigned};

/// The terminating type for `UInt`; it always comes after the most significant
/// bit. `UTerm` by itself represents zero, which is aliased to `U0`.
#[derive(Eq, PartialEq, Ord, PartialOrd, Clone, Copy, Hash, Debug, Default)]
pub struct UTerm;

impl UTerm {
    /// Instantiates a singleton representing this unsigned integer.
    #[inline]
    pub fn new() -> UTerm {
        UTerm
    }
}

impl Unsigned for UTerm {
    const U8: u8 = 0;
    const U16: u16 = 0;
    const U32: u32 = 0;
    const U64: u64 = 0;
    #[cfg(feature = "i128")]
    const U128: u128 = 0;
    const USIZE: usize = 0;

    const I8: i8 = 0;
    const I16: i16 = 0;
    const I32: i32 = 0;
    const I64: i64 = 0;
    #[cfg(feature = "i128")]
    const I128: i128 = 0;
    const ISIZE: isize = 0;

    #[inline]
    fn to_u8() -> u8 {
        0
    }
    #[inline]
    fn to_u16() -> u16 {
        0
    }
    #[inline]
    fn to_u32() -> u32 {
        0
    }
    #[inline]
    fn to_u64() -> u64 {
        0
    }
    #[cfg(feature = "i128")]
    #[inline]
    fn to_u128() -> u128 {
        0
    }
    #[inline]
    fn to_usize() -> usize {
        0
    }

    #[inline]
    fn to_i8() -> i8 {
        0
    }
    #[inline]
    fn to_i16() -> i16 {
        0
    }
    #[inline]
    fn to_i32() -> i32 {
        0
    }
    #[inline]
    fn to_i64() -> i64 {
        0
    }
    #[cfg(feature = "i128")]
    #[inline]
    fn to_i128() -> i128 {
        0
    }
    #[inline]
    fn to_isize() -> isize {
        0
    }
}

/// `UInt` is defined recursively, where `B` is the least significant bit and `U` is the rest
/// of the number. Conceptually, `U` should be bound by the trait `Unsigned` and `B` should
/// be bound by the trait `Bit`, but enforcing these bounds causes linear instead of
/// logrithmic scaling in some places, so they are left off for now. They may be enforced in
/// future.
///
/// In order to keep numbers unique, leading zeros are not allowed, so `UInt<UTerm, B0>` is
/// forbidden.
///
/// # Example
/// ```rust
/// use typenum::{B0, B1, UInt, UTerm};
///
/// # #[allow(dead_code)]
/// type U6 = UInt<UInt<UInt<UTerm, B1>, B1>, B0>;
/// ```
#[derive(Eq, PartialEq, Ord, PartialOrd, Clone, Copy, Hash, Debug, Default)]
pub struct UInt<U, B> {
    _marker: PhantomData<(U, B)>,
}

impl<U: Unsigned, B: Bit> UInt<U, B> {
    /// Instantiates a singleton representing this unsigned integer.
    #[inline]
    pub fn new() -> UInt<U, B> {
        UInt {
            _marker: PhantomData,
        }
    }
}

impl<U: Unsigned, B: Bit> Unsigned for UInt<U, B> {
    const U8: u8 = B::U8 | U::U8 << 1;
    const U16: u16 = B::U8 as u16 | U::U16 << 1;
    const U32: u32 = B::U8 as u32 | U::U32 << 1;
    const U64: u64 = B::U8 as u64 | U::U64 << 1;
    #[cfg(feature = "i128")]
    const U128: u128 = B::U8 as u128 | U::U128 << 1;
    const USIZE: usize = B::U8 as usize | U::USIZE << 1;

    const I8: i8 = B::U8 as i8 | U::I8 << 1;
    const I16: i16 = B::U8 as i16 | U::I16 << 1;
    const I32: i32 = B::U8 as i32 | U::I32 << 1;
    const I64: i64 = B::U8 as i64 | U::I64 << 1;
    #[cfg(feature = "i128")]
    const I128: i128 = B::U8 as i128 | U::I128 << 1;
    const ISIZE: isize = B::U8 as isize | U::ISIZE << 1;

    #[inline]
    fn to_u8() -> u8 {
        B::to_u8() | U::to_u8() << 1
    }
    #[inline]
    fn to_u16() -> u16 {
        u16::from(B::to_u8()) | U::to_u16() << 1
    }
    #[inline]
    fn to_u32() -> u32 {
        u32::from(B::to_u8()) | U::to_u32() << 1
    }
    #[inline]
    fn to_u64() -> u64 {
        u64::from(B::to_u8()) | U::to_u64() << 1
    }
    #[cfg(feature = "i128")]
    #[inline]
    fn to_u128() -> u128 {
        u128::from(B::to_u8()) | U::to_u128() << 1
    }
    #[inline]
    fn to_usize() -> usize {
        usize::from(B::to_u8()) | U::to_usize() << 1
    }

    #[inline]
    fn to_i8() -> i8 {
        B::to_u8() as i8 | U::to_i8() << 1
    }
    #[inline]
    fn to_i16() -> i16 {
        i16::from(B::to_u8()) | U::to_i16() << 1
    }
    #[inline]
    fn to_i32() -> i32 {
        i32::from(B::to_u8()) | U::to_i32() << 1
    }
    #[inline]
    fn to_i64() -> i64 {
        i64::from(B::to_u8()) | U::to_i64() << 1
    }
    #[cfg(feature = "i128")]
    #[inline]
    fn to_i128() -> i128 {
        i128::from(B::to_u8()) | U::to_i128() << 1
    }
    #[inline]
    fn to_isize() -> isize {
        B::to_u8() as isize | U::to_isize() << 1
    }
}

impl<U: Unsigned, B: Bit> NonZero for UInt<U, B> {}

impl PowerOfTwo for UInt<UTerm, B1> {}
impl<U: Unsigned + PowerOfTwo> PowerOfTwo for UInt<U, B0> {}

// ---------------------------------------------------------------------------------------
// Getting length of unsigned integers, which is defined as the number of bits before `UTerm`

/// Length of `UTerm` by itself is 0
impl Len for UTerm {
    type Output = U0;
    fn len(&self) -> Self::Output {
        UTerm
    }
}

/// Length of a bit is 1
impl<U: Unsigned, B: Bit> Len for UInt<U, B>
where
    U: Len,
    Length<U>: Add<B1>,
    Add1<Length<U>>: Unsigned,
{
    type Output = Add1<Length<U>>;
    fn len(&self) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Adding bits to unsigned integers

/// `UTerm + B0 = UTerm`
impl Add<B0> for UTerm {
    type Output = UTerm;
    fn add(self, _: B0) -> Self::Output {
        UTerm
    }
}

/// `U + B0 = U`
impl<U: Unsigned, B: Bit> Add<B0> for UInt<U, B> {
    type Output = UInt<U, B>;
    fn add(self, _: B0) -> Self::Output {
        UInt::new()
    }
}

/// `UTerm + B1 = UInt<UTerm, B1>`
impl Add<B1> for UTerm {
    type Output = UInt<UTerm, B1>;
    fn add(self, _: B1) -> Self::Output {
        UInt::new()
    }
}

/// `UInt<U, B0> + B1 = UInt<U + B1>`
impl<U: Unsigned> Add<B1> for UInt<U, B0> {
    type Output = UInt<U, B1>;
    fn add(self, _: B1) -> Self::Output {
        UInt::new()
    }
}

/// `UInt<U, B1> + B1 = UInt<U + B1, B0>`
impl<U: Unsigned> Add<B1> for UInt<U, B1>
where
    U: Add<B1>,
    Add1<U>: Unsigned,
{
    type Output = UInt<Add1<U>, B0>;
    fn add(self, _: B1) -> Self::Output {
        UInt::new()
    }
}

// ---------------------------------------------------------------------------------------
// Adding unsigned integers

/// `UTerm + U = U`
impl<U: Unsigned> Add<U> for UTerm {
    type Output = U;
    fn add(self, _: U) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `UInt<U, B> + UTerm = UInt<U, B>`
impl<U: Unsigned, B: Bit> Add<UTerm> for UInt<U, B> {
    type Output = UInt<U, B>;
    fn add(self, _: UTerm) -> Self::Output {
        UInt::new()
    }
}

/// `UInt<Ul, B0> + UInt<Ur, B0> = UInt<Ul + Ur, B0>`
impl<Ul: Unsigned, Ur: Unsigned> Add<UInt<Ur, B0>> for UInt<Ul, B0>
where
    Ul: Add<Ur>,
{
    type Output = UInt<Sum<Ul, Ur>, B0>;
    fn add(self, _: UInt<Ur, B0>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `UInt<Ul, B0> + UInt<Ur, B1> = UInt<Ul + Ur, B1>`
impl<Ul: Unsigned, Ur: Unsigned> Add<UInt<Ur, B1>> for UInt<Ul, B0>
where
    Ul: Add<Ur>,
{
    type Output = UInt<Sum<Ul, Ur>, B1>;
    fn add(self, _: UInt<Ur, B1>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `UInt<Ul, B1> + UInt<Ur, B0> = UInt<Ul + Ur, B1>`
impl<Ul: Unsigned, Ur: Unsigned> Add<UInt<Ur, B0>> for UInt<Ul, B1>
where
    Ul: Add<Ur>,
{
    type Output = UInt<Sum<Ul, Ur>, B1>;
    fn add(self, _: UInt<Ur, B0>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `UInt<Ul, B1> + UInt<Ur, B1> = UInt<(Ul + Ur) + B1, B0>`
impl<Ul: Unsigned, Ur: Unsigned> Add<UInt<Ur, B1>> for UInt<Ul, B1>
where
    Ul: Add<Ur>,
    Sum<Ul, Ur>: Add<B1>,
{
    type Output = UInt<Add1<Sum<Ul, Ur>>, B0>;
    fn add(self, _: UInt<Ur, B1>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Subtracting bits from unsigned integers

/// `UTerm - B0 = Term`
impl Sub<B0> for UTerm {
    type Output = UTerm;
    fn sub(self, _: B0) -> Self::Output {
        UTerm
    }
}

/// `UInt - B0 = UInt`
impl<U: Unsigned, B: Bit> Sub<B0> for UInt<U, B> {
    type Output = UInt<U, B>;
    fn sub(self, _: B0) -> Self::Output {
        UInt::new()
    }
}

/// `UInt<U, B1> - B1 = UInt<U, B0>`
impl<U: Unsigned, B: Bit> Sub<B1> for UInt<UInt<U, B>, B1> {
    type Output = UInt<UInt<U, B>, B0>;
    fn sub(self, _: B1) -> Self::Output {
        UInt::new()
    }
}

/// `UInt<UTerm, B1> - B1 = UTerm`
impl Sub<B1> for UInt<UTerm, B1> {
    type Output = UTerm;
    fn sub(self, _: B1) -> Self::Output {
        UTerm
    }
}

/// `UInt<U, B0> - B1 = UInt<U - B1, B1>`
impl<U: Unsigned> Sub<B1> for UInt<U, B0>
where
    U: Sub<B1>,
    Sub1<U>: Unsigned,
{
    type Output = UInt<Sub1<U>, B1>;
    fn sub(self, _: B1) -> Self::Output {
        UInt::new()
    }
}

// ---------------------------------------------------------------------------------------
// Subtracting unsigned integers

/// `UTerm - UTerm = UTerm`
impl Sub<UTerm> for UTerm {
    type Output = UTerm;
    fn sub(self, _: UTerm) -> Self::Output {
        UTerm
    }
}

/// Subtracting unsigned integers. We just do our `PrivateSub` and then `Trim` the output.
impl<Ul: Unsigned, Bl: Bit, Ur: Unsigned> Sub<Ur> for UInt<Ul, Bl>
where
    UInt<Ul, Bl>: PrivateSub<Ur>,
    PrivateSubOut<UInt<Ul, Bl>, Ur>: Trim,
{
    type Output = TrimOut<PrivateSubOut<UInt<Ul, Bl>, Ur>>;
    fn sub(self, _: Ur) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `U - UTerm = U`
impl<U: Unsigned> PrivateSub<UTerm> for U {
    type Output = U;
}

/// `UInt<Ul, B0> - UInt<Ur, B0> = UInt<Ul - Ur, B0>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateSub<UInt<Ur, B0>> for UInt<Ul, B0>
where
    Ul: PrivateSub<Ur>,
{
    type Output = UInt<PrivateSubOut<Ul, Ur>, B0>;
}

/// `UInt<Ul, B0> - UInt<Ur, B1> = UInt<(Ul - Ur) - B1, B1>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateSub<UInt<Ur, B1>> for UInt<Ul, B0>
where
    Ul: PrivateSub<Ur>,
    PrivateSubOut<Ul, Ur>: Sub<B1>,
{
    type Output = UInt<Sub1<PrivateSubOut<Ul, Ur>>, B1>;
}

/// `UInt<Ul, B1> - UInt<Ur, B0> = UInt<Ul - Ur, B1>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateSub<UInt<Ur, B0>> for UInt<Ul, B1>
where
    Ul: PrivateSub<Ur>,
{
    type Output = UInt<PrivateSubOut<Ul, Ur>, B1>;
}

/// `UInt<Ul, B1> - UInt<Ur, B1> = UInt<Ul - Ur, B0>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateSub<UInt<Ur, B1>> for UInt<Ul, B1>
where
    Ul: PrivateSub<Ur>,
{
    type Output = UInt<PrivateSubOut<Ul, Ur>, B0>;
}

// ---------------------------------------------------------------------------------------
// And unsigned integers

/// 0 & X = 0
impl<Ur: Unsigned> BitAnd<Ur> for UTerm {
    type Output = UTerm;
    fn bitand(self, _: Ur) -> Self::Output {
        UTerm
    }
}

/// Anding unsigned integers.
/// We use our `PrivateAnd` operator and then `Trim` the output.
impl<Ul: Unsigned, Bl: Bit, Ur: Unsigned> BitAnd<Ur> for UInt<Ul, Bl>
where
    UInt<Ul, Bl>: PrivateAnd<Ur>,
    PrivateAndOut<UInt<Ul, Bl>, Ur>: Trim,
{
    type Output = TrimOut<PrivateAndOut<UInt<Ul, Bl>, Ur>>;
    fn bitand(self, _: Ur) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `UTerm & X = UTerm`
impl<U: Unsigned> PrivateAnd<U> for UTerm {
    type Output = UTerm;
}

/// `X & UTerm = UTerm`
impl<B: Bit, U: Unsigned> PrivateAnd<UTerm> for UInt<U, B> {
    type Output = UTerm;
}

/// `UInt<Ul, B0> & UInt<Ur, B0> = UInt<Ul & Ur, B0>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateAnd<UInt<Ur, B0>> for UInt<Ul, B0>
where
    Ul: PrivateAnd<Ur>,
{
    type Output = UInt<PrivateAndOut<Ul, Ur>, B0>;
}

/// `UInt<Ul, B0> & UInt<Ur, B1> = UInt<Ul & Ur, B0>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateAnd<UInt<Ur, B1>> for UInt<Ul, B0>
where
    Ul: PrivateAnd<Ur>,
{
    type Output = UInt<PrivateAndOut<Ul, Ur>, B0>;
}

/// `UInt<Ul, B1> & UInt<Ur, B0> = UInt<Ul & Ur, B0>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateAnd<UInt<Ur, B0>> for UInt<Ul, B1>
where
    Ul: PrivateAnd<Ur>,
{
    type Output = UInt<PrivateAndOut<Ul, Ur>, B0>;
}

/// `UInt<Ul, B1> & UInt<Ur, B1> = UInt<Ul & Ur, B1>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateAnd<UInt<Ur, B1>> for UInt<Ul, B1>
where
    Ul: PrivateAnd<Ur>,
{
    type Output = UInt<PrivateAndOut<Ul, Ur>, B1>;
}

// ---------------------------------------------------------------------------------------
// Or unsigned integers

/// `UTerm | X = X`
impl<U: Unsigned> BitOr<U> for UTerm {
    type Output = U;
    fn bitor(self, _: U) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

///  `X | UTerm = X`
impl<B: Bit, U: Unsigned> BitOr<UTerm> for UInt<U, B> {
    type Output = Self;
    fn bitor(self, _: UTerm) -> Self::Output {
        UInt::new()
    }
}

/// `UInt<Ul, B0> | UInt<Ur, B0> = UInt<Ul | Ur, B0>`
impl<Ul: Unsigned, Ur: Unsigned> BitOr<UInt<Ur, B0>> for UInt<Ul, B0>
where
    Ul: BitOr<Ur>,
{
    type Output = UInt<<Ul as BitOr<Ur>>::Output, B0>;
    fn bitor(self, _: UInt<Ur, B0>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `UInt<Ul, B0> | UInt<Ur, B1> = UInt<Ul | Ur, B1>`
impl<Ul: Unsigned, Ur: Unsigned> BitOr<UInt<Ur, B1>> for UInt<Ul, B0>
where
    Ul: BitOr<Ur>,
{
    type Output = UInt<Or<Ul, Ur>, B1>;
    fn bitor(self, _: UInt<Ur, B1>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `UInt<Ul, B1> | UInt<Ur, B0> = UInt<Ul | Ur, B1>`
impl<Ul: Unsigned, Ur: Unsigned> BitOr<UInt<Ur, B0>> for UInt<Ul, B1>
where
    Ul: BitOr<Ur>,
{
    type Output = UInt<Or<Ul, Ur>, B1>;
    fn bitor(self, _: UInt<Ur, B0>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `UInt<Ul, B1> | UInt<Ur, B1> = UInt<Ul | Ur, B1>`
impl<Ul: Unsigned, Ur: Unsigned> BitOr<UInt<Ur, B1>> for UInt<Ul, B1>
where
    Ul: BitOr<Ur>,
{
    type Output = UInt<Or<Ul, Ur>, B1>;
    fn bitor(self, _: UInt<Ur, B1>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Xor unsigned integers

/// 0 ^ X = X
impl<Ur: Unsigned> BitXor<Ur> for UTerm {
    type Output = Ur;
    fn bitxor(self, _: Ur) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// Xoring unsigned integers.
/// We use our `PrivateXor` operator and then `Trim` the output.
impl<Ul: Unsigned, Bl: Bit, Ur: Unsigned> BitXor<Ur> for UInt<Ul, Bl>
where
    UInt<Ul, Bl>: PrivateXor<Ur>,
    PrivateXorOut<UInt<Ul, Bl>, Ur>: Trim,
{
    type Output = TrimOut<PrivateXorOut<UInt<Ul, Bl>, Ur>>;
    fn bitxor(self, _: Ur) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `UTerm ^ X = X`
impl<U: Unsigned> PrivateXor<U> for UTerm {
    type Output = U;
}

/// `X ^ UTerm = X`
impl<B: Bit, U: Unsigned> PrivateXor<UTerm> for UInt<U, B> {
    type Output = Self;
}

/// `UInt<Ul, B0> ^ UInt<Ur, B0> = UInt<Ul ^ Ur, B0>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateXor<UInt<Ur, B0>> for UInt<Ul, B0>
where
    Ul: PrivateXor<Ur>,
{
    type Output = UInt<PrivateXorOut<Ul, Ur>, B0>;
}

/// `UInt<Ul, B0> ^ UInt<Ur, B1> = UInt<Ul ^ Ur, B1>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateXor<UInt<Ur, B1>> for UInt<Ul, B0>
where
    Ul: PrivateXor<Ur>,
{
    type Output = UInt<PrivateXorOut<Ul, Ur>, B1>;
}

/// `UInt<Ul, B1> ^ UInt<Ur, B0> = UInt<Ul ^ Ur, B1>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateXor<UInt<Ur, B0>> for UInt<Ul, B1>
where
    Ul: PrivateXor<Ur>,
{
    type Output = UInt<PrivateXorOut<Ul, Ur>, B1>;
}

/// `UInt<Ul, B1> ^ UInt<Ur, B1> = UInt<Ul ^ Ur, B0>`
impl<Ul: Unsigned, Ur: Unsigned> PrivateXor<UInt<Ur, B1>> for UInt<Ul, B1>
where
    Ul: PrivateXor<Ur>,
{
    type Output = UInt<PrivateXorOut<Ul, Ur>, B0>;
}

// ---------------------------------------------------------------------------------------
// Shl unsigned integers

/// Shifting `UTerm` by a 0 bit: `UTerm << B0 = UTerm`
impl Shl<B0> for UTerm {
    type Output = UTerm;
    fn shl(self, _: B0) -> Self::Output {
        UTerm
    }
}

/// Shifting `UTerm` by a 1 bit: `UTerm << B1 = UTerm`
impl Shl<B1> for UTerm {
    type Output = UTerm;
    fn shl(self, _: B1) -> Self::Output {
        UTerm
    }
}

/// Shifting left any unsigned by a zero bit: `U << B0 = U`
impl<U: Unsigned, B: Bit> Shl<B0> for UInt<U, B> {
    type Output = UInt<U, B>;
    fn shl(self, _: B0) -> Self::Output {
        UInt::new()
    }
}

/// Shifting left a `UInt` by a one bit: `UInt<U, B> << B1 = UInt<UInt<U, B>, B0>`
impl<U: Unsigned, B: Bit> Shl<B1> for UInt<U, B> {
    type Output = UInt<UInt<U, B>, B0>;
    fn shl(self, _: B1) -> Self::Output {
        UInt::new()
    }
}

/// Shifting left `UInt` by `UTerm`: `UInt<U, B> << UTerm = UInt<U, B>`
impl<U: Unsigned, B: Bit> Shl<UTerm> for UInt<U, B> {
    type Output = UInt<U, B>;
    fn shl(self, _: UTerm) -> Self::Output {
        UInt::new()
    }
}

/// Shifting left `UTerm` by an unsigned integer: `UTerm << U = UTerm`
impl<U: Unsigned> Shl<U> for UTerm {
    type Output = UTerm;
    fn shl(self, _: U) -> Self::Output {
        UTerm
    }
}

/// Shifting left `UInt` by `UInt`: `X << Y` = `UInt(X, B0) << (Y - 1)`
impl<U: Unsigned, B: Bit, Ur: Unsigned, Br: Bit> Shl<UInt<Ur, Br>> for UInt<U, B>
where
    UInt<Ur, Br>: Sub<B1>,
    UInt<UInt<U, B>, B0>: Shl<Sub1<UInt<Ur, Br>>>,
{
    type Output = Shleft<UInt<UInt<U, B>, B0>, Sub1<UInt<Ur, Br>>>;
    fn shl(self, _: UInt<Ur, Br>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Shr unsigned integers

/// Shifting right a `UTerm` by an unsigned integer: `UTerm >> U = UTerm`
impl<U: Unsigned> Shr<U> for UTerm {
    type Output = UTerm;
    fn shr(self, _: U) -> Self::Output {
        UTerm
    }
}

/// Shifting right `UInt` by `UTerm`: `UInt<U, B> >> UTerm = UInt<U, B>`
impl<U: Unsigned, B: Bit> Shr<UTerm> for UInt<U, B> {
    type Output = UInt<U, B>;
    fn shr(self, _: UTerm) -> Self::Output {
        UInt::new()
    }
}

/// Shifting right `UTerm` by a 0 bit: `UTerm >> B0 = UTerm`
impl Shr<B0> for UTerm {
    type Output = UTerm;
    fn shr(self, _: B0) -> Self::Output {
        UTerm
    }
}

/// Shifting right `UTerm` by a 1 bit: `UTerm >> B1 = UTerm`
impl Shr<B1> for UTerm {
    type Output = UTerm;
    fn shr(self, _: B1) -> Self::Output {
        UTerm
    }
}

/// Shifting right any unsigned by a zero bit: `U >> B0 = U`
impl<U: Unsigned, B: Bit> Shr<B0> for UInt<U, B> {
    type Output = UInt<U, B>;
    fn shr(self, _: B0) -> Self::Output {
        UInt::new()
    }
}

/// Shifting right a `UInt` by a 1 bit: `UInt<U, B> >> B1 = U`
impl<U: Unsigned, B: Bit> Shr<B1> for UInt<U, B> {
    type Output = U;
    fn shr(self, _: B1) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// Shifting right `UInt` by `UInt`: `UInt(U, B) >> Y` = `U >> (Y - 1)`
impl<U: Unsigned, B: Bit, Ur: Unsigned, Br: Bit> Shr<UInt<Ur, Br>> for UInt<U, B>
where
    UInt<Ur, Br>: Sub<B1>,
    U: Shr<Sub1<UInt<Ur, Br>>>,
{
    type Output = Shright<U, Sub1<UInt<Ur, Br>>>;
    fn shr(self, _: UInt<Ur, Br>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Multiply unsigned integers

/// `UInt * B0 = UTerm`
impl<U: Unsigned, B: Bit> Mul<B0> for UInt<U, B> {
    type Output = UTerm;
    fn mul(self, _: B0) -> Self::Output {
        UTerm
    }
}

/// `UTerm * B0 = UTerm`
impl Mul<B0> for UTerm {
    type Output = UTerm;
    fn mul(self, _: B0) -> Self::Output {
        UTerm
    }
}

/// `UTerm * B1 = UTerm`
impl Mul<B1> for UTerm {
    type Output = UTerm;
    fn mul(self, _: B1) -> Self::Output {
        UTerm
    }
}

/// `UInt * B1 = UInt`
impl<U: Unsigned, B: Bit> Mul<B1> for UInt<U, B> {
    type Output = UInt<U, B>;
    fn mul(self, _: B1) -> Self::Output {
        UInt::new()
    }
}

/// `UInt<U, B> * UTerm = UTerm`
impl<U: Unsigned, B: Bit> Mul<UTerm> for UInt<U, B> {
    type Output = UTerm;
    fn mul(self, _: UTerm) -> Self::Output {
        UTerm
    }
}

/// `UTerm * U = UTerm`
impl<U: Unsigned> Mul<U> for UTerm {
    type Output = UTerm;
    fn mul(self, _: U) -> Self::Output {
        UTerm
    }
}

/// `UInt<Ul, B0> * UInt<Ur, B> = UInt<(Ul * UInt<Ur, B>), B0>`
impl<Ul: Unsigned, B: Bit, Ur: Unsigned> Mul<UInt<Ur, B>> for UInt<Ul, B0>
where
    Ul: Mul<UInt<Ur, B>>,
{
    type Output = UInt<Prod<Ul, UInt<Ur, B>>, B0>;
    fn mul(self, _: UInt<Ur, B>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `UInt<Ul, B1> * UInt<Ur, B> = UInt<(Ul * UInt<Ur, B>), B0> + UInt<Ur, B>`
impl<Ul: Unsigned, B: Bit, Ur: Unsigned> Mul<UInt<Ur, B>> for UInt<Ul, B1>
where
    Ul: Mul<UInt<Ur, B>>,
    UInt<Prod<Ul, UInt<Ur, B>>, B0>: Add<UInt<Ur, B>>,
{
    type Output = Sum<UInt<Prod<Ul, UInt<Ur, B>>, B0>, UInt<Ur, B>>;
    fn mul(self, _: UInt<Ur, B>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Compare unsigned integers

/// Zero == Zero
impl Cmp<UTerm> for UTerm {
    type Output = Equal;
}

/// Nonzero > Zero
impl<U: Unsigned, B: Bit> Cmp<UTerm> for UInt<U, B> {
    type Output = Greater;
}

/// Zero < Nonzero
impl<U: Unsigned, B: Bit> Cmp<UInt<U, B>> for UTerm {
    type Output = Less;
}

/// `UInt<Ul, B0>` cmp with `UInt<Ur, B0>`: `SoFar` is `Equal`
impl<Ul: Unsigned, Ur: Unsigned> Cmp<UInt<Ur, B0>> for UInt<Ul, B0>
where
    Ul: PrivateCmp<Ur, Equal>,
{
    type Output = PrivateCmpOut<Ul, Ur, Equal>;
}

/// `UInt<Ul, B1>` cmp with `UInt<Ur, B1>`: `SoFar` is `Equal`
impl<Ul: Unsigned, Ur: Unsigned> Cmp<UInt<Ur, B1>> for UInt<Ul, B1>
where
    Ul: PrivateCmp<Ur, Equal>,
{
    type Output = PrivateCmpOut<Ul, Ur, Equal>;
}

/// `UInt<Ul, B0>` cmp with `UInt<Ur, B1>`: `SoFar` is `Less`
impl<Ul: Unsigned, Ur: Unsigned> Cmp<UInt<Ur, B1>> for UInt<Ul, B0>
where
    Ul: PrivateCmp<Ur, Less>,
{
    type Output = PrivateCmpOut<Ul, Ur, Less>;
}

/// `UInt<Ul, B1>` cmp with `UInt<Ur, B0>`: `SoFar` is `Greater`
impl<Ul: Unsigned, Ur: Unsigned> Cmp<UInt<Ur, B0>> for UInt<Ul, B1>
where
    Ul: PrivateCmp<Ur, Greater>,
{
    type Output = PrivateCmpOut<Ul, Ur, Greater>;
}

/// Comparing non-terimal bits, with both having bit `B0`.
/// These are `Equal`, so we propogate `SoFar`.
impl<Ul, Ur, SoFar> PrivateCmp<UInt<Ur, B0>, SoFar> for UInt<Ul, B0>
where
    Ul: Unsigned,
    Ur: Unsigned,
    SoFar: Ord,
    Ul: PrivateCmp<Ur, SoFar>,
{
    type Output = PrivateCmpOut<Ul, Ur, SoFar>;
}

/// Comparing non-terimal bits, with both having bit `B1`.
/// These are `Equal`, so we propogate `SoFar`.
impl<Ul, Ur, SoFar> PrivateCmp<UInt<Ur, B1>, SoFar> for UInt<Ul, B1>
where
    Ul: Unsigned,
    Ur: Unsigned,
    SoFar: Ord,
    Ul: PrivateCmp<Ur, SoFar>,
{
    type Output = PrivateCmpOut<Ul, Ur, SoFar>;
}

/// Comparing non-terimal bits, with `Lhs` having bit `B0` and `Rhs` having bit `B1`.
/// `SoFar`, Lhs is `Less`.
impl<Ul, Ur, SoFar> PrivateCmp<UInt<Ur, B1>, SoFar> for UInt<Ul, B0>
where
    Ul: Unsigned,
    Ur: Unsigned,
    SoFar: Ord,
    Ul: PrivateCmp<Ur, Less>,
{
    type Output = PrivateCmpOut<Ul, Ur, Less>;
}

/// Comparing non-terimal bits, with `Lhs` having bit `B1` and `Rhs` having bit `B0`.
/// `SoFar`, Lhs is `Greater`.
impl<Ul, Ur, SoFar> PrivateCmp<UInt<Ur, B0>, SoFar> for UInt<Ul, B1>
where
    Ul: Unsigned,
    Ur: Unsigned,
    SoFar: Ord,
    Ul: PrivateCmp<Ur, Greater>,
{
    type Output = PrivateCmpOut<Ul, Ur, Greater>;
}

/// Got to the end of just the `Lhs`. It's `Less`.
impl<U: Unsigned, B: Bit, SoFar: Ord> PrivateCmp<UInt<U, B>, SoFar> for UTerm {
    type Output = Less;
}

/// Got to the end of just the `Rhs`. `Lhs` is `Greater`.
impl<U: Unsigned, B: Bit, SoFar: Ord> PrivateCmp<UTerm, SoFar> for UInt<U, B> {
    type Output = Greater;
}

/// Got to the end of both! Return `SoFar`
impl<SoFar: Ord> PrivateCmp<UTerm, SoFar> for UTerm {
    type Output = SoFar;
}

// ---------------------------------------------------------------------------------------
// Getting difference in number of bits

impl<Ul, Bl, Ur, Br> BitDiff<UInt<Ur, Br>> for UInt<Ul, Bl>
where
    Ul: Unsigned,
    Bl: Bit,
    Ur: Unsigned,
    Br: Bit,
    Ul: BitDiff<Ur>,
{
    type Output = BitDiffOut<Ul, Ur>;
}

impl<Ul> BitDiff<UTerm> for Ul
where
    Ul: Unsigned + Len,
{
    type Output = Length<Ul>;
}

// ---------------------------------------------------------------------------------------
// Shifting one number until it's the size of another
use private::ShiftDiff;
impl<Ul: Unsigned, Ur: Unsigned> ShiftDiff<Ur> for Ul
where
    Ur: BitDiff<Ul>,
    Ul: Shl<BitDiffOut<Ur, Ul>>,
{
    type Output = Shleft<Ul, BitDiffOut<Ur, Ul>>;
}

// ---------------------------------------------------------------------------------------
// Powers of unsigned integers

/// X^N
impl<X: Unsigned, N: Unsigned> Pow<N> for X
where
    X: PrivatePow<U1, N>,
{
    type Output = PrivatePowOut<X, U1, N>;
    fn powi(self, _: N) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

impl<Y: Unsigned, X: Unsigned> PrivatePow<Y, U0> for X {
    type Output = Y;
}

impl<Y: Unsigned, X: Unsigned> PrivatePow<Y, U1> for X
where
    X: Mul<Y>,
{
    type Output = Prod<X, Y>;
}

/// N is even
impl<Y: Unsigned, U: Unsigned, B: Bit, X: Unsigned> PrivatePow<Y, UInt<UInt<U, B>, B0>> for X
where
    X: Mul,
    Square<X>: PrivatePow<Y, UInt<U, B>>,
{
    type Output = PrivatePowOut<Square<X>, Y, UInt<U, B>>;
}

/// N is odd
impl<Y: Unsigned, U: Unsigned, B: Bit, X: Unsigned> PrivatePow<Y, UInt<UInt<U, B>, B1>> for X
where
    X: Mul + Mul<Y>,
    Square<X>: PrivatePow<Prod<X, Y>, UInt<U, B>>,
{
    type Output = PrivatePowOut<Square<X>, Prod<X, Y>, UInt<U, B>>;
}

// -----------------------------------------
// GetBit

#[allow(missing_docs)]
pub trait GetBit<I> {
    #[allow(missing_docs)]
    type Output;
}

#[allow(missing_docs)]
pub type GetBitOut<N, I> = <N as GetBit<I>>::Output;

// Base case
impl<Un, Bn> GetBit<U0> for UInt<Un, Bn> {
    type Output = Bn;
}

// Recursion case
impl<Un, Bn, Ui, Bi> GetBit<UInt<Ui, Bi>> for UInt<Un, Bn>
where
    UInt<Ui, Bi>: Sub<B1>,
    Un: GetBit<Sub1<UInt<Ui, Bi>>>,
{
    type Output = GetBitOut<Un, Sub1<UInt<Ui, Bi>>>;
}

// Ran out of bits
impl<I> GetBit<I> for UTerm {
    type Output = B0;
}

#[test]
fn test_get_bit() {
    use consts::*;
    use Same;
    type T1 = <GetBitOut<U2, U0> as Same<B0>>::Output;
    type T2 = <GetBitOut<U2, U1> as Same<B1>>::Output;
    type T3 = <GetBitOut<U2, U2> as Same<B0>>::Output;

    <T1 as Bit>::to_bool();
    <T2 as Bit>::to_bool();
    <T3 as Bit>::to_bool();
}

// -----------------------------------------
// SetBit

/// A **type operator** that, when implemented for unsigned integer `N`, sets the bit at position
/// `I` to `B`.
pub trait SetBit<I, B> {
    #[allow(missing_docs)]
    type Output;
}
/// Alias for the result of calling `SetBit`: `SetBitOut<N, I, B> = <N as SetBit<I, B>>::Output`.
pub type SetBitOut<N, I, B> = <N as SetBit<I, B>>::Output;

use private::{PrivateSetBit, PrivateSetBitOut};

// Call private one then trim it
impl<N, I, B> SetBit<I, B> for N
where
    N: PrivateSetBit<I, B>,
    PrivateSetBitOut<N, I, B>: Trim,
{
    type Output = TrimOut<PrivateSetBitOut<N, I, B>>;
}

// Base case
impl<Un, Bn, B> PrivateSetBit<U0, B> for UInt<Un, Bn> {
    type Output = UInt<Un, B>;
}

// Recursion case
impl<Un, Bn, Ui, Bi, B> PrivateSetBit<UInt<Ui, Bi>, B> for UInt<Un, Bn>
where
    UInt<Ui, Bi>: Sub<B1>,
    Un: PrivateSetBit<Sub1<UInt<Ui, Bi>>, B>,
{
    type Output = UInt<PrivateSetBitOut<Un, Sub1<UInt<Ui, Bi>>, B>, Bn>;
}

// Ran out of bits, setting B0
impl<I> PrivateSetBit<I, B0> for UTerm {
    type Output = UTerm;
}

// Ran out of bits, setting B1
impl<I> PrivateSetBit<I, B1> for UTerm
where
    U1: Shl<I>,
{
    type Output = Shleft<U1, I>;
}

#[test]
fn test_set_bit() {
    use consts::*;
    use Same;
    type T1 = <SetBitOut<U2, U0, B0> as Same<U2>>::Output;
    type T2 = <SetBitOut<U2, U0, B1> as Same<U3>>::Output;
    type T3 = <SetBitOut<U2, U1, B0> as Same<U0>>::Output;
    type T4 = <SetBitOut<U2, U1, B1> as Same<U2>>::Output;
    type T5 = <SetBitOut<U2, U2, B0> as Same<U2>>::Output;
    type T6 = <SetBitOut<U2, U2, B1> as Same<U6>>::Output;
    type T7 = <SetBitOut<U2, U3, B0> as Same<U2>>::Output;
    type T8 = <SetBitOut<U2, U3, B1> as Same<U10>>::Output;
    type T9 = <SetBitOut<U2, U4, B0> as Same<U2>>::Output;
    type T10 = <SetBitOut<U2, U4, B1> as Same<U18>>::Output;

    type T11 = <SetBitOut<U3, U0, B0> as Same<U2>>::Output;

    <T1 as Unsigned>::to_u32();
    <T2 as Unsigned>::to_u32();
    <T3 as Unsigned>::to_u32();
    <T4 as Unsigned>::to_u32();
    <T5 as Unsigned>::to_u32();
    <T6 as Unsigned>::to_u32();
    <T7 as Unsigned>::to_u32();
    <T8 as Unsigned>::to_u32();
    <T9 as Unsigned>::to_u32();
    <T10 as Unsigned>::to_u32();
    <T11 as Unsigned>::to_u32();
}

// -----------------------------------------

// Division algorithm:
// We have N / D:
// let Q = 0, R = 0
// NBits = len(N)
// for I in NBits-1..0:
//   R <<=1
//   R[0] = N[i]
//   let C = R.cmp(D)
//   if C == Equal or Greater:
//     R -= D
//     Q[i] = 1

#[cfg(tests)]
mod tests {
    macro_rules! test_div {
        ($a:ident / $b:ident = $c:ident) => (
            {
                type R = Quot<$a, $b>;
                assert_eq!(<R as Unsigned>::to_usize(), $c::to_usize());
            }
        );
    }
    #[test]
    fn test_div() {
        use consts::*;
        use {Quot, Same};

        test_div!(U0 / U1 = U0);
        test_div!(U1 / U1 = U1);
        test_div!(U2 / U1 = U2);
        test_div!(U3 / U1 = U3);
        test_div!(U4 / U1 = U4);

        test_div!(U0 / U2 = U0);
        test_div!(U1 / U2 = U0);
        test_div!(U2 / U2 = U1);
        test_div!(U3 / U2 = U1);
        test_div!(U4 / U2 = U2);
        test_div!(U6 / U2 = U3);
        test_div!(U7 / U2 = U3);

        type T = <SetBitOut<U0, U1, B1> as Same<U2>>::Output;
        <T as Unsigned>::to_u32();
    }
}
// -----------------------------------------
// Div
use core::ops::Div;

// 0 // N
impl<Ur: Unsigned, Br: Bit> Div<UInt<Ur, Br>> for UTerm {
    type Output = UTerm;
    fn div(self, _: UInt<Ur, Br>) -> Self::Output {
        UTerm
    }
}

// M // N
impl<Ul: Unsigned, Bl: Bit, Ur: Unsigned, Br: Bit> Div<UInt<Ur, Br>> for UInt<Ul, Bl>
where
    UInt<Ul, Bl>: Len,
    Length<UInt<Ul, Bl>>: Sub<B1>,
    (): PrivateDiv<UInt<Ul, Bl>, UInt<Ur, Br>, U0, U0, Sub1<Length<UInt<Ul, Bl>>>>,
{
    type Output = PrivateDivQuot<UInt<Ul, Bl>, UInt<Ur, Br>, U0, U0, Sub1<Length<UInt<Ul, Bl>>>>;
    fn div(self, _: UInt<Ur, Br>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// -----------------------------------------
// Rem
use core::ops::Rem;

// 0 % N
impl<Ur: Unsigned, Br: Bit> Rem<UInt<Ur, Br>> for UTerm {
    type Output = UTerm;
    fn rem(self, _: UInt<Ur, Br>) -> Self::Output {
        UTerm
    }
}

// M % N
impl<Ul: Unsigned, Bl: Bit, Ur: Unsigned, Br: Bit> Rem<UInt<Ur, Br>> for UInt<Ul, Bl>
where
    UInt<Ul, Bl>: Len,
    Length<UInt<Ul, Bl>>: Sub<B1>,
    (): PrivateDiv<UInt<Ul, Bl>, UInt<Ur, Br>, U0, U0, Sub1<Length<UInt<Ul, Bl>>>>,
{
    type Output = PrivateDivRem<UInt<Ul, Bl>, UInt<Ur, Br>, U0, U0, Sub1<Length<UInt<Ul, Bl>>>>;
    fn rem(self, _: UInt<Ur, Br>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// -----------------------------------------
// PrivateDiv
use private::{PrivateDiv, PrivateDivQuot, PrivateDivRem};

use Compare;
// R == 0: We set R = UInt<UTerm, N[i]>, then call out to PrivateDivIf for the if statement
impl<N, D, Q, I> PrivateDiv<N, D, Q, U0, I> for ()
where
    N: GetBit<I>,
    UInt<UTerm, GetBitOut<N, I>>: Trim,
    TrimOut<UInt<UTerm, GetBitOut<N, I>>>: Cmp<D>,
    (): PrivateDivIf<
        N,
        D,
        Q,
        TrimOut<UInt<UTerm, GetBitOut<N, I>>>,
        I,
        Compare<TrimOut<UInt<UTerm, GetBitOut<N, I>>>, D>,
    >,
{
    type Quotient = PrivateDivIfQuot<
        N,
        D,
        Q,
        TrimOut<UInt<UTerm, GetBitOut<N, I>>>,
        I,
        Compare<TrimOut<UInt<UTerm, GetBitOut<N, I>>>, D>,
    >;
    type Remainder = PrivateDivIfRem<
        N,
        D,
        Q,
        TrimOut<UInt<UTerm, GetBitOut<N, I>>>,
        I,
        Compare<TrimOut<UInt<UTerm, GetBitOut<N, I>>>, D>,
    >;
}

// R > 0: We perform R <<= 1 and R[0] = N[i], then call out to PrivateDivIf for the if statement
impl<N, D, Q, Ur, Br, I> PrivateDiv<N, D, Q, UInt<Ur, Br>, I> for ()
where
    N: GetBit<I>,
    UInt<UInt<Ur, Br>, GetBitOut<N, I>>: Cmp<D>,
    (): PrivateDivIf<
        N,
        D,
        Q,
        UInt<UInt<Ur, Br>, GetBitOut<N, I>>,
        I,
        Compare<UInt<UInt<Ur, Br>, GetBitOut<N, I>>, D>,
    >,
{
    type Quotient = PrivateDivIfQuot<
        N,
        D,
        Q,
        UInt<UInt<Ur, Br>, GetBitOut<N, I>>,
        I,
        Compare<UInt<UInt<Ur, Br>, GetBitOut<N, I>>, D>,
    >;
    type Remainder = PrivateDivIfRem<
        N,
        D,
        Q,
        UInt<UInt<Ur, Br>, GetBitOut<N, I>>,
        I,
        Compare<UInt<UInt<Ur, Br>, GetBitOut<N, I>>, D>,
    >;
}

// -----------------------------------------
// PrivateDivIf

use private::{PrivateDivIf, PrivateDivIfQuot, PrivateDivIfRem};

// R < D, I > 0, we do nothing and recurse
impl<N, D, Q, R, Ui, Bi> PrivateDivIf<N, D, Q, R, UInt<Ui, Bi>, Less> for ()
where
    UInt<Ui, Bi>: Sub<B1>,
    (): PrivateDiv<N, D, Q, R, Sub1<UInt<Ui, Bi>>>,
{
    type Quotient = PrivateDivQuot<N, D, Q, R, Sub1<UInt<Ui, Bi>>>;
    type Remainder = PrivateDivRem<N, D, Q, R, Sub1<UInt<Ui, Bi>>>;
}

// R == D, I > 0, we set R = 0, Q[I] = 1 and recurse
impl<N, D, Q, R, Ui, Bi> PrivateDivIf<N, D, Q, R, UInt<Ui, Bi>, Equal> for ()
where
    UInt<Ui, Bi>: Sub<B1>,
    Q: SetBit<UInt<Ui, Bi>, B1>,
    (): PrivateDiv<N, D, SetBitOut<Q, UInt<Ui, Bi>, B1>, U0, Sub1<UInt<Ui, Bi>>>,
{
    type Quotient = PrivateDivQuot<N, D, SetBitOut<Q, UInt<Ui, Bi>, B1>, U0, Sub1<UInt<Ui, Bi>>>;
    type Remainder = PrivateDivRem<N, D, SetBitOut<Q, UInt<Ui, Bi>, B1>, U0, Sub1<UInt<Ui, Bi>>>;
}

use Diff;
// R > D, I > 0, we set R -= D, Q[I] = 1 and recurse
impl<N, D, Q, R, Ui, Bi> PrivateDivIf<N, D, Q, R, UInt<Ui, Bi>, Greater> for ()
where
    UInt<Ui, Bi>: Sub<B1>,
    R: Sub<D>,
    Q: SetBit<UInt<Ui, Bi>, B1>,
    (): PrivateDiv<N, D, SetBitOut<Q, UInt<Ui, Bi>, B1>, Diff<R, D>, Sub1<UInt<Ui, Bi>>>,
{
    type Quotient =
        PrivateDivQuot<N, D, SetBitOut<Q, UInt<Ui, Bi>, B1>, Diff<R, D>, Sub1<UInt<Ui, Bi>>>;
    type Remainder =
        PrivateDivRem<N, D, SetBitOut<Q, UInt<Ui, Bi>, B1>, Diff<R, D>, Sub1<UInt<Ui, Bi>>>;
}

// R < D, I == 0: we do nothing, and return
impl<N, D, Q, R> PrivateDivIf<N, D, Q, R, U0, Less> for () {
    type Quotient = Q;
    type Remainder = R;
}

// R == D, I == 0: we set R = 0, Q[I] = 1, and return
impl<N, D, Q, R> PrivateDivIf<N, D, Q, R, U0, Equal> for ()
where
    Q: SetBit<U0, B1>,
{
    type Quotient = SetBitOut<Q, U0, B1>;
    type Remainder = U0;
}

// R > D, I == 0: We set R -= D, Q[I] = 1, and return
impl<N, D, Q, R> PrivateDivIf<N, D, Q, R, U0, Greater> for ()
where
    R: Sub<D>,
    Q: SetBit<U0, B1>,
{
    type Quotient = SetBitOut<Q, U0, B1>;
    type Remainder = Diff<R, D>;
}

// -----------------------------------------
// PartialDiv
use {PartialDiv, Quot};
impl<Ur: Unsigned, Br: Bit> PartialDiv<UInt<Ur, Br>> for UTerm {
    type Output = UTerm;
    fn partial_div(self, _: UInt<Ur, Br>) -> Self::Output {
        UTerm
    }
}

// M / N
impl<Ul: Unsigned, Bl: Bit, Ur: Unsigned, Br: Bit> PartialDiv<UInt<Ur, Br>> for UInt<Ul, Bl>
where
    UInt<Ul, Bl>: Div<UInt<Ur, Br>> + Rem<UInt<Ur, Br>, Output = U0>,
{
    type Output = Quot<UInt<Ul, Bl>, UInt<Ur, Br>>;
    fn partial_div(self, _: UInt<Ur, Br>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// -----------------------------------------
// PrivateMin
use private::{PrivateMin, PrivateMinOut};

impl<U, B, Ur> PrivateMin<Ur, Equal> for UInt<U, B>
where
    Ur: Unsigned,
    U: Unsigned,
    B: Bit,
{
    type Output = UInt<U, B>;
    fn private_min(self, _: Ur) -> Self::Output {
        self
    }
}

impl<U, B, Ur> PrivateMin<Ur, Less> for UInt<U, B>
where
    Ur: Unsigned,
    U: Unsigned,
    B: Bit,
{
    type Output = UInt<U, B>;
    fn private_min(self, _: Ur) -> Self::Output {
        self
    }
}

impl<U, B, Ur> PrivateMin<Ur, Greater> for UInt<U, B>
where
    Ur: Unsigned,
    U: Unsigned,
    B: Bit,
{
    type Output = Ur;
    fn private_min(self, rhs: Ur) -> Self::Output {
        rhs
    }
}

// -----------------------------------------
// Min
use Min;

impl<U> Min<U> for UTerm
where
    U: Unsigned,
{
    type Output = UTerm;
    fn min(self, _: U) -> Self::Output {
        self
    }
}

impl<U, B, Ur> Min<Ur> for UInt<U, B>
where
    U: Unsigned,
    B: Bit,
    Ur: Unsigned,
    UInt<U, B>: Cmp<Ur> + PrivateMin<Ur, Compare<UInt<U, B>, Ur>>,
{
    type Output = PrivateMinOut<UInt<U, B>, Ur, Compare<UInt<U, B>, Ur>>;
    fn min(self, rhs: Ur) -> Self::Output {
        self.private_min(rhs)
    }
}

// -----------------------------------------
// PrivateMax
use private::{PrivateMax, PrivateMaxOut};

impl<U, B, Ur> PrivateMax<Ur, Equal> for UInt<U, B>
where
    Ur: Unsigned,
    U: Unsigned,
    B: Bit,
{
    type Output = UInt<U, B>;
    fn private_max(self, _: Ur) -> Self::Output {
        self
    }
}

impl<U, B, Ur> PrivateMax<Ur, Less> for UInt<U, B>
where
    Ur: Unsigned,
    U: Unsigned,
    B: Bit,
{
    type Output = Ur;
    fn private_max(self, rhs: Ur) -> Self::Output {
        rhs
    }
}

impl<U, B, Ur> PrivateMax<Ur, Greater> for UInt<U, B>
where
    Ur: Unsigned,
    U: Unsigned,
    B: Bit,
{
    type Output = UInt<U, B>;
    fn private_max(self, _: Ur) -> Self::Output {
        self
    }
}

// -----------------------------------------
// Max
use Max;

impl<U> Max<U> for UTerm
where
    U: Unsigned,
{
    type Output = U;
    fn max(self, rhs: U) -> Self::Output {
        rhs
    }
}

impl<U, B, Ur> Max<Ur> for UInt<U, B>
where
    U: Unsigned,
    B: Bit,
    Ur: Unsigned,
    UInt<U, B>: Cmp<Ur> + PrivateMax<Ur, Compare<UInt<U, B>, Ur>>,
{
    type Output = PrivateMaxOut<UInt<U, B>, Ur, Compare<UInt<U, B>, Ur>>;
    fn max(self, rhs: Ur) -> Self::Output {
        self.private_max(rhs)
    }
}
