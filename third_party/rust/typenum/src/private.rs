//! **Ignore me!** This module is for things that are conceptually private but that must
//! be made public for typenum to work correctly.
//!
//! Unless you are working on typenum itself, **there is no need to view anything here**.
//!
//! Certainly don't implement any of the traits here for anything.
//!
//!
//! Just look away.
//!
//!
//! Loooooooooooooooooooooooooooooooooook awaaaaaaaaaaaayyyyyyyyyyyyyyyyyyyyyyyyyyyyy...
//!
//!
//! If you do manage to find something of use in here, please let me know. If you can make a
//! compelling case, it may be moved out of __private.
//!
//! Note: Aliases for private type operators will all be named simply that operator followed
//! by an abbreviated name of its associated type.
//!

#![doc(hidden)]

// use ::{Sub};
use bit::{B0, B1, Bit};
use uint::{UInt, UTerm, Unsigned};

/// Convenience trait. Calls `Invert` -> `TrimTrailingZeros` -> `Invert`
pub trait Trim {
    type Output;
}
pub type TrimOut<A> = <A as Trim>::Output;

/// Gets rid of all zeros until it hits a one.

// ONLY IMPLEMENT FOR INVERTED NUMBERS!
pub trait TrimTrailingZeros {
    type Output;
}
pub type TrimTrailingZerosOut<A> = <A as TrimTrailingZeros>::Output;

/// Converts between standard numbers and inverted ones that have the most significant
/// digit on the outside.
pub trait Invert {
    type Output;
}
pub type InvertOut<A> = <A as Invert>::Output;

/// Doubly private! Called by invert to make the magic happen once its done the first step.
/// The Rhs is what we've got so far.
pub trait PrivateInvert<Rhs> {
    type Output;
}
pub type PrivateInvertOut<A, Rhs> = <A as PrivateInvert<Rhs>>::Output;

/// Terminating character for `InvertedUInt`s
pub enum InvertedUTerm {}

/// Inverted `UInt` (has most significant digit on the outside)
pub struct InvertedUInt<IU: InvertedUnsigned, B: Bit> {
    _marker: (IU, B),
}

/// Does the real anding for `UInt`s; `And` just calls this and then `Trim`.
pub trait PrivateAnd<Rhs = Self> {
    type Output;
}
pub type PrivateAndOut<A, Rhs> = <A as PrivateAnd<Rhs>>::Output;

/// Does the real xoring for `UInt`s; `Xor` just calls this and then `Trim`.
pub trait PrivateXor<Rhs = Self> {
    type Output;
}
pub type PrivateXorOut<A, Rhs> = <A as PrivateXor<Rhs>>::Output;

/// Does the real subtraction for `UInt`s; `Sub` just calls this and then `Trim`.
pub trait PrivateSub<Rhs = Self> {
    type Output;
}
pub type PrivateSubOut<A, Rhs> = <A as PrivateSub<Rhs>>::Output;

/// Used for addition of signed integers; `C = P.cmp(N)`
/// Assumes `P = Self` is positive and `N` is negative
/// where `P` and `N` are both passed as unsigned integers
pub trait PrivateIntegerAdd<C, N> {
    type Output;
}
pub type PrivateIntegerAddOut<P, C, N> = <P as PrivateIntegerAdd<C, N>>::Output;

pub trait PrivatePow<Y, N> {
    type Output;
}
pub type PrivatePowOut<A, Y, N> = <A as PrivatePow<Y, N>>::Output;

/// Performs `Shl` on `Lhs` so that `SizeOf(Lhs) = SizeOf(Rhs)`
/// Fails if `SizeOf(Lhs) > SizeOf(Rhs)`
pub trait ShiftDiff<Rhs> {
    type Output;
}
pub type ShiftDiffOut<A, Rhs> = <A as ShiftDiff<Rhs>>::Output;

/// Gives `SizeOf(Lhs) - SizeOf(Rhs)`
pub trait BitDiff<Rhs> {
    type Output;
}
pub type BitDiffOut<A, Rhs> = <A as BitDiff<Rhs>>::Output;

/// Inverted unsigned numbers
pub trait InvertedUnsigned {
    fn to_u64() -> u64;
}

impl InvertedUnsigned for InvertedUTerm {
    fn to_u64() -> u64 {
        0
    }
}

impl<IU: InvertedUnsigned, B: Bit> InvertedUnsigned for InvertedUInt<IU, B> {
    fn to_u64() -> u64 {
        u64::from(B::to_u8()) | IU::to_u64() << 1
    }
}

impl Invert for UTerm {
    type Output = InvertedUTerm;
}

impl<U: Unsigned, B: Bit> Invert for UInt<U, B>
where
    U: PrivateInvert<InvertedUInt<InvertedUTerm, B>>,
{
    type Output = PrivateInvertOut<U, InvertedUInt<InvertedUTerm, B>>;
}

impl<IU: InvertedUnsigned> PrivateInvert<IU> for UTerm {
    type Output = IU;
}

impl<IU: InvertedUnsigned, U: Unsigned, B: Bit> PrivateInvert<IU> for UInt<U, B>
where
    U: PrivateInvert<InvertedUInt<IU, B>>,
{
    type Output = PrivateInvertOut<U, InvertedUInt<IU, B>>;
}

#[test]
fn test_inversion() {
    type Test4 = <::consts::U4 as Invert>::Output;
    type Test5 = <::consts::U5 as Invert>::Output;
    type Test12 = <::consts::U12 as Invert>::Output;
    type Test16 = <::consts::U16 as Invert>::Output;

    assert_eq!(1, <Test4 as InvertedUnsigned>::to_u64());
    assert_eq!(5, <Test5 as InvertedUnsigned>::to_u64());
    assert_eq!(3, <Test12 as InvertedUnsigned>::to_u64());
    assert_eq!(1, <Test16 as InvertedUnsigned>::to_u64());
}

impl Invert for InvertedUTerm {
    type Output = UTerm;
}

impl<IU: InvertedUnsigned, B: Bit> Invert for InvertedUInt<IU, B>
where
    IU: PrivateInvert<UInt<UTerm, B>>,
{
    type Output = <IU as PrivateInvert<UInt<UTerm, B>>>::Output;
}

impl<U: Unsigned> PrivateInvert<U> for InvertedUTerm {
    type Output = U;
}

impl<U: Unsigned, IU: InvertedUnsigned, B: Bit> PrivateInvert<U> for InvertedUInt<IU, B>
where
    IU: PrivateInvert<UInt<U, B>>,
{
    type Output = <IU as PrivateInvert<UInt<U, B>>>::Output;
}

#[test]
fn test_double_inversion() {
    type Test4 = <<::consts::U4 as Invert>::Output as Invert>::Output;
    type Test5 = <<::consts::U5 as Invert>::Output as Invert>::Output;
    type Test12 = <<::consts::U12 as Invert>::Output as Invert>::Output;
    type Test16 = <<::consts::U16 as Invert>::Output as Invert>::Output;

    assert_eq!(4, <Test4 as Unsigned>::to_u64());
    assert_eq!(5, <Test5 as Unsigned>::to_u64());
    assert_eq!(12, <Test12 as Unsigned>::to_u64());
    assert_eq!(16, <Test16 as Unsigned>::to_u64());
}

impl TrimTrailingZeros for InvertedUTerm {
    type Output = InvertedUTerm;
}

impl<IU: InvertedUnsigned> TrimTrailingZeros for InvertedUInt<IU, B1> {
    type Output = Self;
}

impl<IU: InvertedUnsigned> TrimTrailingZeros for InvertedUInt<IU, B0>
where
    IU: TrimTrailingZeros,
{
    type Output = <IU as TrimTrailingZeros>::Output;
}

impl<U: Unsigned> Trim for U
where
    U: Invert,
    <U as Invert>::Output: TrimTrailingZeros,
    <<U as Invert>::Output as TrimTrailingZeros>::Output: Invert,
{
    type Output = <<<U as Invert>::Output as TrimTrailingZeros>::Output as Invert>::Output;
}

// Note: Trimming is tested when we do subtraction.

pub trait PrivateCmp<Rhs, SoFar> {
    type Output;
}
pub type PrivateCmpOut<A, Rhs, SoFar> = <A as PrivateCmp<Rhs, SoFar>>::Output;

// Set Bit
pub trait PrivateSetBit<I, B> {
    type Output;
}
pub type PrivateSetBitOut<N, I, B> = <N as PrivateSetBit<I, B>>::Output;

// Div
pub trait PrivateDiv<N, D, Q, R, I> {
    type Quotient;
    type Remainder;
}

pub type PrivateDivQuot<N, D, Q, R, I> = <() as PrivateDiv<N, D, Q, R, I>>::Quotient;
pub type PrivateDivRem<N, D, Q, R, I> = <() as PrivateDiv<N, D, Q, R, I>>::Remainder;

pub trait PrivateDivIf<N, D, Q, R, I, RcmpD> {
    type Quotient;
    type Remainder;
}

pub type PrivateDivIfQuot<N, D, Q, R, I, RcmpD> =
    <() as PrivateDivIf<N, D, Q, R, I, RcmpD>>::Quotient;
pub type PrivateDivIfRem<N, D, Q, R, I, RcmpD> =
    <() as PrivateDivIf<N, D, Q, R, I, RcmpD>>::Remainder;

// Div for signed ints
pub trait PrivateDivInt<C, Divisor> {
    type Output;
}
pub type PrivateDivIntOut<A, C, Divisor> = <A as PrivateDivInt<C, Divisor>>::Output;

pub trait PrivateRem<URem, Divisor> {
    type Output;
}
pub type PrivateRemOut<A, URem, Divisor> = <A as PrivateRem<URem, Divisor>>::Output;

// min max
pub trait PrivateMin<Rhs, CmpResult> {
    type Output;
    fn private_min(self, rhs: Rhs) -> Self::Output;
}
pub type PrivateMinOut<A, B, CmpResult> = <A as PrivateMin<B, CmpResult>>::Output;

pub trait PrivateMax<Rhs, CmpResult> {
    type Output;
    fn private_max(self, rhs: Rhs) -> Self::Output;
}
pub type PrivateMaxOut<A, B, CmpResult> = <A as PrivateMax<B, CmpResult>>::Output;

// Comparisons

use {Equal, False, Greater, Less, True};

pub trait IsLessPrivate<Rhs, Cmp> {
    type Output: Bit;
}

impl<A, B> IsLessPrivate<B, Less> for A {
    type Output = True;
}
impl<A, B> IsLessPrivate<B, Equal> for A {
    type Output = False;
}
impl<A, B> IsLessPrivate<B, Greater> for A {
    type Output = False;
}

pub trait IsEqualPrivate<Rhs, Cmp> {
    type Output: Bit;
}

impl<A, B> IsEqualPrivate<B, Less> for A {
    type Output = False;
}
impl<A, B> IsEqualPrivate<B, Equal> for A {
    type Output = True;
}
impl<A, B> IsEqualPrivate<B, Greater> for A {
    type Output = False;
}

pub trait IsGreaterPrivate<Rhs, Cmp> {
    type Output: Bit;
}

impl<A, B> IsGreaterPrivate<B, Less> for A {
    type Output = False;
}
impl<A, B> IsGreaterPrivate<B, Equal> for A {
    type Output = False;
}
impl<A, B> IsGreaterPrivate<B, Greater> for A {
    type Output = True;
}

pub trait IsLessOrEqualPrivate<Rhs, Cmp> {
    type Output: Bit;
}

impl<A, B> IsLessOrEqualPrivate<B, Less> for A {
    type Output = True;
}
impl<A, B> IsLessOrEqualPrivate<B, Equal> for A {
    type Output = True;
}
impl<A, B> IsLessOrEqualPrivate<B, Greater> for A {
    type Output = False;
}

pub trait IsNotEqualPrivate<Rhs, Cmp> {
    type Output: Bit;
}

impl<A, B> IsNotEqualPrivate<B, Less> for A {
    type Output = True;
}
impl<A, B> IsNotEqualPrivate<B, Equal> for A {
    type Output = False;
}
impl<A, B> IsNotEqualPrivate<B, Greater> for A {
    type Output = True;
}

pub trait IsGreaterOrEqualPrivate<Rhs, Cmp> {
    type Output: Bit;
}

impl<A, B> IsGreaterOrEqualPrivate<B, Less> for A {
    type Output = False;
}
impl<A, B> IsGreaterOrEqualPrivate<B, Equal> for A {
    type Output = True;
}
impl<A, B> IsGreaterOrEqualPrivate<B, Greater> for A {
    type Output = True;
}
