//! Type-level signed integers.
//!
//!
//! Type **operators** implemented:
//!
//! From `core::ops`: `Add`, `Sub`, `Mul`, `Div`, and `Rem`.
//! From `typenum`: `Same`, `Cmp`, and `Pow`.
//!
//! Rather than directly using the structs defined in this module, it is recommended that
//! you import and use the relevant aliases from the [consts](../consts/index.html) module.
//!
//! Note that operators that work on the underlying structure of the number are
//! intentionally not implemented. This is because this implementation of signed integers
//! does *not* use twos-complement, and implementing them would require making arbitrary
//! choices, causing the results of such operators to be difficult to reason about.
//!
//! # Example
//! ```rust
//! use std::ops::{Add, Sub, Mul, Div, Rem};
//! use typenum::{Integer, N3, P2};
//!
//! assert_eq!(<N3 as Add<P2>>::Output::to_i32(), -1);
//! assert_eq!(<N3 as Sub<P2>>::Output::to_i32(), -5);
//! assert_eq!(<N3 as Mul<P2>>::Output::to_i32(), -6);
//! assert_eq!(<N3 as Div<P2>>::Output::to_i32(), -1);
//! assert_eq!(<N3 as Rem<P2>>::Output::to_i32(), -1);
//! ```
//!

use core::ops::{Add, Div, Mul, Neg, Rem, Sub};
use core::marker::PhantomData;

use {Cmp, Equal, Greater, Less, NonZero, Pow, PowerOfTwo};
use uint::{UInt, Unsigned};
use bit::{B0, B1, Bit};
use private::{PrivateDivInt, PrivateIntegerAdd, PrivateRem};
use consts::{N1, P1, U0, U1};

pub use marker_traits::Integer;

/// Type-level signed integers with positive sign.
#[derive(Eq, PartialEq, Ord, PartialOrd, Clone, Copy, Hash, Debug, Default)]
pub struct PInt<U: Unsigned + NonZero> {
    _marker: PhantomData<U>,
}

/// Type-level signed integers with negative sign.
#[derive(Eq, PartialEq, Ord, PartialOrd, Clone, Copy, Hash, Debug, Default)]
pub struct NInt<U: Unsigned + NonZero> {
    _marker: PhantomData<U>,
}

impl<U: Unsigned + NonZero> PInt<U> {
    /// Instantiates a singleton representing this strictly positive integer.
    #[inline]
    pub fn new() -> PInt<U> {
        PInt {
            _marker: PhantomData,
        }
    }
}

impl<U: Unsigned + NonZero> NInt<U> {
    /// Instantiates a singleton representing this strictly negative integer.
    #[inline]
    pub fn new() -> NInt<U> {
        NInt {
            _marker: PhantomData,
        }
    }
}

/// The type-level signed integer 0.
#[derive(Eq, PartialEq, Ord, PartialOrd, Clone, Copy, Hash, Debug, Default)]
pub struct Z0;

impl Z0 {
    /// Instantiates a singleton representing the integer 0.
    #[inline]
    pub fn new() -> Z0 {
        Z0
    }
}

impl<U: Unsigned + NonZero> NonZero for PInt<U> {}
impl<U: Unsigned + NonZero> NonZero for NInt<U> {}

impl<U: Unsigned + NonZero + PowerOfTwo> PowerOfTwo for PInt<U> {}

impl Integer for Z0 {
    const I8: i8 = 0;
    const I16: i16 = 0;
    const I32: i32 = 0;
    const I64: i64 = 0;
    #[cfg(feature = "i128")]
    const I128: i128 = 0;
    const ISIZE: isize = 0;

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

impl<U: Unsigned + NonZero> Integer for PInt<U> {
    const I8: i8 = U::I8;
    const I16: i16 = U::I16;
    const I32: i32 = U::I32;
    const I64: i64 = U::I64;
    #[cfg(feature = "i128")]
    const I128: i128 = U::I128;
    const ISIZE: isize = U::ISIZE;

    #[inline]
    fn to_i8() -> i8 {
        <U as Unsigned>::to_i8()
    }
    #[inline]
    fn to_i16() -> i16 {
        <U as Unsigned>::to_i16()
    }
    #[inline]
    fn to_i32() -> i32 {
        <U as Unsigned>::to_i32()
    }
    #[inline]
    fn to_i64() -> i64 {
        <U as Unsigned>::to_i64()
    }
    #[cfg(feature = "i128")]
    #[inline]
    fn to_i128() -> i128 {
        <U as Unsigned>::to_i128()
    }
    #[inline]
    fn to_isize() -> isize {
        <U as Unsigned>::to_isize()
    }
}

impl<U: Unsigned + NonZero> Integer for NInt<U> {
    const I8: i8 = -U::I8;
    const I16: i16 = -U::I16;
    const I32: i32 = -U::I32;
    const I64: i64 = -U::I64;
    #[cfg(feature = "i128")]
    const I128: i128 = -U::I128;
    const ISIZE: isize = -U::ISIZE;

    #[inline]
    fn to_i8() -> i8 {
        -<U as Unsigned>::to_i8()
    }
    #[inline]
    fn to_i16() -> i16 {
        -<U as Unsigned>::to_i16()
    }
    #[inline]
    fn to_i32() -> i32 {
        -<U as Unsigned>::to_i32()
    }
    #[inline]
    fn to_i64() -> i64 {
        -<U as Unsigned>::to_i64()
    }
    #[cfg(feature = "i128")]
    #[inline]
    fn to_i128() -> i128 {
        -<U as Unsigned>::to_i128()
    }
    #[inline]
    fn to_isize() -> isize {
        -<U as Unsigned>::to_isize()
    }
}

// ---------------------------------------------------------------------------------------
// Neg

/// `-Z0 = Z0`
impl Neg for Z0 {
    type Output = Z0;
    fn neg(self) -> Self::Output {
        Z0
    }
}

/// `-PInt = NInt`
impl<U: Unsigned + NonZero> Neg for PInt<U> {
    type Output = NInt<U>;
    fn neg(self) -> Self::Output {
        NInt::new()
    }
}

/// `-NInt = PInt`
impl<U: Unsigned + NonZero> Neg for NInt<U> {
    type Output = PInt<U>;
    fn neg(self) -> Self::Output {
        PInt::new()
    }
}

// ---------------------------------------------------------------------------------------
// Add

/// `Z0 + I = I`
impl<I: Integer> Add<I> for Z0 {
    type Output = I;
    fn add(self, _: I) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `PInt + Z0 = PInt`
impl<U: Unsigned + NonZero> Add<Z0> for PInt<U> {
    type Output = PInt<U>;
    fn add(self, _: Z0) -> Self::Output {
        PInt::new()
    }
}

/// `NInt + Z0 = NInt`
impl<U: Unsigned + NonZero> Add<Z0> for NInt<U> {
    type Output = NInt<U>;
    fn add(self, _: Z0) -> Self::Output {
        NInt::new()
    }
}

/// `P(Ul) + P(Ur) = P(Ul + Ur)`
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Add<PInt<Ur>> for PInt<Ul>
where
    Ul: Add<Ur>,
    <Ul as Add<Ur>>::Output: Unsigned + NonZero,
{
    type Output = PInt<<Ul as Add<Ur>>::Output>;
    fn add(self, _: PInt<Ur>) -> Self::Output {
        PInt::new()
    }
}

/// `N(Ul) + N(Ur) = N(Ul + Ur)`
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Add<NInt<Ur>> for NInt<Ul>
where
    Ul: Add<Ur>,
    <Ul as Add<Ur>>::Output: Unsigned + NonZero,
{
    type Output = NInt<<Ul as Add<Ur>>::Output>;
    fn add(self, _: NInt<Ur>) -> Self::Output {
        NInt::new()
    }
}

/// `P(Ul) + N(Ur)`: We resolve this with our `PrivateAdd`
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Add<NInt<Ur>> for PInt<Ul>
where
    Ul: Cmp<Ur> + PrivateIntegerAdd<<Ul as Cmp<Ur>>::Output, Ur>,
{
    type Output = <Ul as PrivateIntegerAdd<<Ul as Cmp<Ur>>::Output, Ur>>::Output;
    fn add(self, _: NInt<Ur>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `N(Ul) + P(Ur)`: We resolve this with our `PrivateAdd`
// We just do the same thing as above, swapping Lhs and Rhs
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Add<PInt<Ur>> for NInt<Ul>
where
    Ur: Cmp<Ul> + PrivateIntegerAdd<<Ur as Cmp<Ul>>::Output, Ul>,
{
    type Output = <Ur as PrivateIntegerAdd<<Ur as Cmp<Ul>>::Output, Ul>>::Output;
    fn add(self, _: PInt<Ur>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `P + N = 0` where `P == N`
impl<N: Unsigned, P: Unsigned> PrivateIntegerAdd<Equal, N> for P {
    type Output = Z0;
}

/// `P + N = Positive` where `P > N`
impl<N: Unsigned, P: Unsigned> PrivateIntegerAdd<Greater, N> for P
where
    P: Sub<N>,
    <P as Sub<N>>::Output: Unsigned + NonZero,
{
    type Output = PInt<<P as Sub<N>>::Output>;
}

/// `P + N = Negative` where `P < N`
impl<N: Unsigned, P: Unsigned> PrivateIntegerAdd<Less, N> for P
where
    N: Sub<P>,
    <N as Sub<P>>::Output: Unsigned + NonZero,
{
    type Output = NInt<<N as Sub<P>>::Output>;
}

// ---------------------------------------------------------------------------------------
// Sub

/// `Z0 - Z0 = Z0`
impl Sub<Z0> for Z0 {
    type Output = Z0;
    fn sub(self, _: Z0) -> Self::Output {
        Z0
    }
}

/// `Z0 - P = N`
impl<U: Unsigned + NonZero> Sub<PInt<U>> for Z0 {
    type Output = NInt<U>;
    fn sub(self, _: PInt<U>) -> Self::Output {
        NInt::new()
    }
}

/// `Z0 - N = P`
impl<U: Unsigned + NonZero> Sub<NInt<U>> for Z0 {
    type Output = PInt<U>;
    fn sub(self, _: NInt<U>) -> Self::Output {
        PInt::new()
    }
}

/// `PInt - Z0 = PInt`
impl<U: Unsigned + NonZero> Sub<Z0> for PInt<U> {
    type Output = PInt<U>;
    fn sub(self, _: Z0) -> Self::Output {
        PInt::new()
    }
}

/// `NInt - Z0 = NInt`
impl<U: Unsigned + NonZero> Sub<Z0> for NInt<U> {
    type Output = NInt<U>;
    fn sub(self, _: Z0) -> Self::Output {
        NInt::new()
    }
}

/// `P(Ul) - N(Ur) = P(Ul + Ur)`
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Sub<NInt<Ur>> for PInt<Ul>
where
    Ul: Add<Ur>,
    <Ul as Add<Ur>>::Output: Unsigned + NonZero,
{
    type Output = PInt<<Ul as Add<Ur>>::Output>;
    fn sub(self, _: NInt<Ur>) -> Self::Output {
        PInt::new()
    }
}

/// `N(Ul) - P(Ur) = N(Ul + Ur)`
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Sub<PInt<Ur>> for NInt<Ul>
where
    Ul: Add<Ur>,
    <Ul as Add<Ur>>::Output: Unsigned + NonZero,
{
    type Output = NInt<<Ul as Add<Ur>>::Output>;
    fn sub(self, _: PInt<Ur>) -> Self::Output {
        NInt::new()
    }
}

/// `P(Ul) - P(Ur)`: We resolve this with our `PrivateAdd`
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Sub<PInt<Ur>> for PInt<Ul>
where
    Ul: Cmp<Ur> + PrivateIntegerAdd<<Ul as Cmp<Ur>>::Output, Ur>,
{
    type Output = <Ul as PrivateIntegerAdd<<Ul as Cmp<Ur>>::Output, Ur>>::Output;
    fn sub(self, _: PInt<Ur>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

/// `N(Ul) - N(Ur)`: We resolve this with our `PrivateAdd`
// We just do the same thing as above, swapping Lhs and Rhs
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Sub<NInt<Ur>> for NInt<Ul>
where
    Ur: Cmp<Ul> + PrivateIntegerAdd<<Ur as Cmp<Ul>>::Output, Ul>,
{
    type Output = <Ur as PrivateIntegerAdd<<Ur as Cmp<Ul>>::Output, Ul>>::Output;
    fn sub(self, _: NInt<Ur>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Mul

/// `Z0 * I = Z0`
impl<I: Integer> Mul<I> for Z0 {
    type Output = Z0;
    fn mul(self, _: I) -> Self::Output {
        Z0
    }
}

/// `P * Z0 = Z0`
impl<U: Unsigned + NonZero> Mul<Z0> for PInt<U> {
    type Output = Z0;
    fn mul(self, _: Z0) -> Self::Output {
        Z0
    }
}

/// `N * Z0 = Z0`
impl<U: Unsigned + NonZero> Mul<Z0> for NInt<U> {
    type Output = Z0;
    fn mul(self, _: Z0) -> Self::Output {
        Z0
    }
}

/// P(Ul) * P(Ur) = P(Ul * Ur)
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Mul<PInt<Ur>> for PInt<Ul>
where
    Ul: Mul<Ur>,
    <Ul as Mul<Ur>>::Output: Unsigned + NonZero,
{
    type Output = PInt<<Ul as Mul<Ur>>::Output>;
    fn mul(self, _: PInt<Ur>) -> Self::Output {
        PInt::new()
    }
}

/// N(Ul) * N(Ur) = P(Ul * Ur)
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Mul<NInt<Ur>> for NInt<Ul>
where
    Ul: Mul<Ur>,
    <Ul as Mul<Ur>>::Output: Unsigned + NonZero,
{
    type Output = PInt<<Ul as Mul<Ur>>::Output>;
    fn mul(self, _: NInt<Ur>) -> Self::Output {
        PInt::new()
    }
}

/// P(Ul) * N(Ur) = N(Ul * Ur)
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Mul<NInt<Ur>> for PInt<Ul>
where
    Ul: Mul<Ur>,
    <Ul as Mul<Ur>>::Output: Unsigned + NonZero,
{
    type Output = NInt<<Ul as Mul<Ur>>::Output>;
    fn mul(self, _: NInt<Ur>) -> Self::Output {
        NInt::new()
    }
}

/// N(Ul) * P(Ur) = N(Ul * Ur)
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Mul<PInt<Ur>> for NInt<Ul>
where
    Ul: Mul<Ur>,
    <Ul as Mul<Ur>>::Output: Unsigned + NonZero,
{
    type Output = NInt<<Ul as Mul<Ur>>::Output>;
    fn mul(self, _: PInt<Ur>) -> Self::Output {
        NInt::new()
    }
}

// ---------------------------------------------------------------------------------------
// Div

/// `Z0 / I = Z0` where `I != 0`
impl<I: Integer + NonZero> Div<I> for Z0 {
    type Output = Z0;
    fn div(self, _: I) -> Self::Output {
        Z0
    }
}

macro_rules! impl_int_div {
    ($A:ident, $B:ident, $R:ident) => (
        /// `$A<Ul> / $B<Ur> = $R<Ul / Ur>`
        impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Div<$B<Ur>> for $A<Ul>
            where Ul: Cmp<Ur>,
                  $A<Ul>: PrivateDivInt<<Ul as Cmp<Ur>>::Output, $B<Ur>>
        {
            type Output = <$A<Ul> as PrivateDivInt<
                <Ul as Cmp<Ur>>::Output,
                $B<Ur>>>::Output;
            fn div(self, _: $B<Ur>) -> Self::Output {
                unsafe { ::core::mem::uninitialized() }
            }
        }
        impl<Ul, Ur> PrivateDivInt<Less, $B<Ur>> for $A<Ul>
            where Ul: Unsigned + NonZero, Ur: Unsigned + NonZero,
        {
            type Output = Z0;
        }
        impl<Ul, Ur> PrivateDivInt<Equal, $B<Ur>> for $A<Ul>
            where Ul: Unsigned + NonZero, Ur: Unsigned + NonZero,
        {
            type Output = $R<U1>;
        }
        impl<Ul, Ur> PrivateDivInt<Greater, $B<Ur>> for $A<Ul>
            where Ul: Unsigned + NonZero + Div<Ur>,
                  Ur: Unsigned + NonZero,
                  <Ul as Div<Ur>>::Output: Unsigned + NonZero,
        {
            type Output = $R<<Ul as Div<Ur>>::Output>;
        }
        );
}

impl_int_div!(PInt, PInt, PInt);
impl_int_div!(PInt, NInt, NInt);
impl_int_div!(NInt, PInt, NInt);
impl_int_div!(NInt, NInt, PInt);

// ---------------------------------------------------------------------------------------
// PartialDiv

use {PartialDiv, Quot};

impl<M, N> PartialDiv<N> for M
where
    M: Integer + Div<N> + Rem<N, Output = Z0>,
{
    type Output = Quot<M, N>;
    fn partial_div(self, _: N) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Cmp

/// 0 == 0
impl Cmp<Z0> for Z0 {
    type Output = Equal;
}

/// 0 > -X
impl<U: Unsigned + NonZero> Cmp<NInt<U>> for Z0 {
    type Output = Greater;
}

/// 0 < X
impl<U: Unsigned + NonZero> Cmp<PInt<U>> for Z0 {
    type Output = Less;
}

/// X > 0
impl<U: Unsigned + NonZero> Cmp<Z0> for PInt<U> {
    type Output = Greater;
}

/// -X < 0
impl<U: Unsigned + NonZero> Cmp<Z0> for NInt<U> {
    type Output = Less;
}

/// -X < Y
impl<P: Unsigned + NonZero, N: Unsigned + NonZero> Cmp<PInt<P>> for NInt<N> {
    type Output = Less;
}

/// X > - Y
impl<P: Unsigned + NonZero, N: Unsigned + NonZero> Cmp<NInt<N>> for PInt<P> {
    type Output = Greater;
}

/// X <==> Y
impl<Pl: Cmp<Pr> + Unsigned + NonZero, Pr: Unsigned + NonZero> Cmp<PInt<Pr>> for PInt<Pl> {
    type Output = <Pl as Cmp<Pr>>::Output;
}

/// -X <==> -Y
impl<Nl: Unsigned + NonZero, Nr: Cmp<Nl> + Unsigned + NonZero> Cmp<NInt<Nr>> for NInt<Nl> {
    type Output = <Nr as Cmp<Nl>>::Output;
}

// ---------------------------------------------------------------------------------------
// Rem

/// `Z0 % I = Z0` where `I != 0`
impl<I: Integer + NonZero> Rem<I> for Z0 {
    type Output = Z0;
    fn rem(self, _: I) -> Self::Output {
        Z0
    }
}

macro_rules! impl_int_rem {
    ($A:ident, $B:ident, $R:ident) => (
        /// `$A<Ul> % $B<Ur> = $R<Ul % Ur>`
        impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Rem<$B<Ur>> for $A<Ul>
            where Ul: Rem<Ur>,
                  $A<Ul>: PrivateRem<<Ul as Rem<Ur>>::Output, $B<Ur>>
        {
            type Output = <$A<Ul> as PrivateRem<
                <Ul as Rem<Ur>>::Output,
                $B<Ur>>>::Output;
            fn rem(self, _: $B<Ur>) -> Self::Output {
                unsafe { ::core::mem::uninitialized() }
            }
        }
        impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> PrivateRem<U0, $B<Ur>> for $A<Ul> {
            type Output = Z0;
        }
        impl<Ul, Ur, U, B> PrivateRem<UInt<U, B>, $B<Ur>> for $A<Ul>
            where Ul: Unsigned + NonZero, Ur: Unsigned + NonZero, U: Unsigned, B: Bit,
        {
            type Output = $R<UInt<U, B>>;
        }
        );
}

impl_int_rem!(PInt, PInt, PInt);
impl_int_rem!(PInt, NInt, PInt);
impl_int_rem!(NInt, PInt, NInt);
impl_int_rem!(NInt, NInt, NInt);

// ---------------------------------------------------------------------------------------
// Pow

/// 0^0 = 1
impl Pow<Z0> for Z0 {
    type Output = P1;
    fn powi(self, _: Z0) -> Self::Output {
        P1::new()
    }
}

/// 0^P = 0
impl<U: Unsigned + NonZero> Pow<PInt<U>> for Z0 {
    type Output = Z0;
    fn powi(self, _: PInt<U>) -> Self::Output {
        Z0
    }
}

/// 0^N = 0
impl<U: Unsigned + NonZero> Pow<NInt<U>> for Z0 {
    type Output = Z0;
    fn powi(self, _: NInt<U>) -> Self::Output {
        Z0
    }
}

/// 1^N = 1
impl<U: Unsigned + NonZero> Pow<NInt<U>> for P1 {
    type Output = P1;
    fn powi(self, _: NInt<U>) -> Self::Output {
        P1::new()
    }
}

/// (-1)^N = 1 if N is even
impl<U: Unsigned> Pow<NInt<UInt<U, B0>>> for N1 {
    type Output = P1;
    fn powi(self, _: NInt<UInt<U, B0>>) -> Self::Output {
        P1::new()
    }
}

/// (-1)^N = -1 if N is odd
impl<U: Unsigned> Pow<NInt<UInt<U, B1>>> for N1 {
    type Output = N1;
    fn powi(self, _: NInt<UInt<U, B1>>) -> Self::Output {
        N1::new()
    }
}

/// P^0 = 1
impl<U: Unsigned + NonZero> Pow<Z0> for PInt<U> {
    type Output = P1;
    fn powi(self, _: Z0) -> Self::Output {
        P1::new()
    }
}

/// N^0 = 1
impl<U: Unsigned + NonZero> Pow<Z0> for NInt<U> {
    type Output = P1;
    fn powi(self, _: Z0) -> Self::Output {
        P1::new()
    }
}

/// P(Ul)^P(Ur) = P(Ul^Ur)
impl<Ul: Unsigned + NonZero, Ur: Unsigned + NonZero> Pow<PInt<Ur>> for PInt<Ul>
where
    Ul: Pow<Ur>,
    <Ul as Pow<Ur>>::Output: Unsigned + NonZero,
{
    type Output = PInt<<Ul as Pow<Ur>>::Output>;
    fn powi(self, _: PInt<Ur>) -> Self::Output {
        PInt::new()
    }
}

/// N(Ul)^P(Ur) = P(Ul^Ur) if Ur is even
impl<Ul: Unsigned + NonZero, Ur: Unsigned> Pow<PInt<UInt<Ur, B0>>> for NInt<Ul>
where
    Ul: Pow<UInt<Ur, B0>>,
    <Ul as Pow<UInt<Ur, B0>>>::Output: Unsigned + NonZero,
{
    type Output = PInt<<Ul as Pow<UInt<Ur, B0>>>::Output>;
    fn powi(self, _: PInt<UInt<Ur, B0>>) -> Self::Output {
        PInt::new()
    }
}

/// N(Ul)^P(Ur) = N(Ul^Ur) if Ur is odd
impl<Ul: Unsigned + NonZero, Ur: Unsigned> Pow<PInt<UInt<Ur, B1>>> for NInt<Ul>
where
    Ul: Pow<UInt<Ur, B1>>,
    <Ul as Pow<UInt<Ur, B1>>>::Output: Unsigned + NonZero,
{
    type Output = NInt<<Ul as Pow<UInt<Ur, B1>>>::Output>;
    fn powi(self, _: PInt<UInt<Ur, B1>>) -> Self::Output {
        NInt::new()
    }
}

// ---------------------------------------------------------------------------------------
// Min
use {Max, Maximum, Min, Minimum};

impl Min<Z0> for Z0 {
    type Output = Z0;
    fn min(self, _: Z0) -> Self::Output {
        self
    }
}

impl<U> Min<PInt<U>> for Z0
where
    U: Unsigned + NonZero,
{
    type Output = Z0;
    fn min(self, _: PInt<U>) -> Self::Output {
        self
    }
}

impl<U> Min<NInt<U>> for Z0
where
    U: Unsigned + NonZero,
{
    type Output = NInt<U>;
    fn min(self, rhs: NInt<U>) -> Self::Output {
        rhs
    }
}

impl<U> Min<Z0> for PInt<U>
where
    U: Unsigned + NonZero,
{
    type Output = Z0;
    fn min(self, rhs: Z0) -> Self::Output {
        rhs
    }
}

impl<U> Min<Z0> for NInt<U>
where
    U: Unsigned + NonZero,
{
    type Output = NInt<U>;
    fn min(self, _: Z0) -> Self::Output {
        self
    }
}

impl<Ul, Ur> Min<PInt<Ur>> for PInt<Ul>
where
    Ul: Unsigned + NonZero + Min<Ur>,
    Ur: Unsigned + NonZero,
    Minimum<Ul, Ur>: Unsigned + NonZero,
{
    type Output = PInt<Minimum<Ul, Ur>>;
    fn min(self, _: PInt<Ur>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

impl<Ul, Ur> Min<PInt<Ur>> for NInt<Ul>
where
    Ul: Unsigned + NonZero,
    Ur: Unsigned + NonZero,
{
    type Output = NInt<Ul>;
    fn min(self, _: PInt<Ur>) -> Self::Output {
        self
    }
}

impl<Ul, Ur> Min<NInt<Ur>> for PInt<Ul>
where
    Ul: Unsigned + NonZero,
    Ur: Unsigned + NonZero,
{
    type Output = NInt<Ur>;
    fn min(self, rhs: NInt<Ur>) -> Self::Output {
        rhs
    }
}

impl<Ul, Ur> Min<NInt<Ur>> for NInt<Ul>
where
    Ul: Unsigned + NonZero + Max<Ur>,
    Ur: Unsigned + NonZero,
    Maximum<Ul, Ur>: Unsigned + NonZero,
{
    type Output = NInt<Maximum<Ul, Ur>>;
    fn min(self, _: NInt<Ur>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Max

impl Max<Z0> for Z0 {
    type Output = Z0;
    fn max(self, _: Z0) -> Self::Output {
        self
    }
}

impl<U> Max<PInt<U>> for Z0
where
    U: Unsigned + NonZero,
{
    type Output = PInt<U>;
    fn max(self, rhs: PInt<U>) -> Self::Output {
        rhs
    }
}

impl<U> Max<NInt<U>> for Z0
where
    U: Unsigned + NonZero,
{
    type Output = Z0;
    fn max(self, _: NInt<U>) -> Self::Output {
        self
    }
}

impl<U> Max<Z0> for PInt<U>
where
    U: Unsigned + NonZero,
{
    type Output = PInt<U>;
    fn max(self, _: Z0) -> Self::Output {
        self
    }
}

impl<U> Max<Z0> for NInt<U>
where
    U: Unsigned + NonZero,
{
    type Output = Z0;
    fn max(self, rhs: Z0) -> Self::Output {
        rhs
    }
}

impl<Ul, Ur> Max<PInt<Ur>> for PInt<Ul>
where
    Ul: Unsigned + NonZero + Max<Ur>,
    Ur: Unsigned + NonZero,
    Maximum<Ul, Ur>: Unsigned + NonZero,
{
    type Output = PInt<Maximum<Ul, Ur>>;
    fn max(self, _: PInt<Ur>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

impl<Ul, Ur> Max<PInt<Ur>> for NInt<Ul>
where
    Ul: Unsigned + NonZero,
    Ur: Unsigned + NonZero,
{
    type Output = PInt<Ur>;
    fn max(self, rhs: PInt<Ur>) -> Self::Output {
        rhs
    }
}

impl<Ul, Ur> Max<NInt<Ur>> for PInt<Ul>
where
    Ul: Unsigned + NonZero,
    Ur: Unsigned + NonZero,
{
    type Output = PInt<Ul>;
    fn max(self, _: NInt<Ur>) -> Self::Output {
        self
    }
}

impl<Ul, Ur> Max<NInt<Ur>> for NInt<Ul>
where
    Ul: Unsigned + NonZero + Min<Ur>,
    Ur: Unsigned + NonZero,
    Minimum<Ul, Ur>: Unsigned + NonZero,
{
    type Output = NInt<Minimum<Ul, Ur>>;
    fn max(self, _: NInt<Ur>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}
