//! A type-level array of type-level numbers.
//!
//! It is not very featureful right now, and should be considered a work in progress.

use core::marker::PhantomData;
use core::ops::{Add, Div, Mul, Sub};

use super::*;

/// The terminating type for type arrays.
#[derive(Eq, PartialEq, Ord, PartialOrd, Clone, Copy, Hash, Debug)]
pub struct ATerm;

impl TypeArray for ATerm {}

/// `TArr` is a type that acts as an array of types. It is defined similarly to `UInt`, only its
/// values can be more than bits, and it is designed to act as an array. So you can only add two if
/// they have the same number of elements, for example.
///
/// This array is only really designed to contain `Integer` types. If you use it with others, you
/// may find it lacking functionality.
#[derive(Eq, PartialEq, Ord, PartialOrd, Clone, Copy, Hash, Debug)]
pub struct TArr<V, A> {
    _marker: PhantomData<(V, A)>,
}

impl<V, A> TypeArray for TArr<V, A> {}

/// Create a new type-level arrray. Only usable on Rust 1.13.0 or newer.
///
/// There's not a whole lot you can do with it right now.
///
/// # Example
/// ```rust
/// #[macro_use]
/// extern crate typenum;
/// use typenum::consts::*;
///
/// type Array = tarr![P3, N4, Z0, P38];
/// # fn main() { let _: Array; }
#[macro_export]
macro_rules! tarr {
    () => ( $crate::ATerm );
    ($n:ty) => ( $crate::TArr<$n, $crate::ATerm> );
    ($n:ty,) => ( $crate::TArr<$n, $crate::ATerm> );
    ($n:ty, $($tail:ty),+) => ( $crate::TArr<$n, tarr![$($tail),+]> );
    ($n:ty, $($tail:ty),+,) => ( $crate::TArr<$n, tarr![$($tail),+]> );
}

// ---------------------------------------------------------------------------------------
// Length

/// Length of `ATerm` by itself is 0
impl Len for ATerm {
    type Output = U0;
    fn len(&self) -> Self::Output {
        UTerm
    }
}

/// Size of a `TypeArray`
impl<V, A> Len for TArr<V, A>
where
    A: Len,
    Length<A>: Add<B1>,
    Sum<Length<A>, B1>: Unsigned,
{
    type Output = Add1<Length<A>>;
    fn len(&self) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Add arrays
// Note that two arrays are only addable if they are the same length.

impl Add<ATerm> for ATerm {
    type Output = ATerm;
    fn add(self, _: ATerm) -> Self::Output {
        ATerm
    }
}

impl<Al, Vl, Ar, Vr> Add<TArr<Vr, Ar>> for TArr<Vl, Al>
where
    Al: Add<Ar>,
    Vl: Add<Vr>,
{
    type Output = TArr<Sum<Vl, Vr>, Sum<Al, Ar>>;
    fn add(self, _: TArr<Vr, Ar>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Subtract arrays
// Note that two arrays are only subtractable if they are the same length.

impl Sub<ATerm> for ATerm {
    type Output = ATerm;
    fn sub(self, _: ATerm) -> Self::Output {
        ATerm
    }
}

impl<Vl, Al, Vr, Ar> Sub<TArr<Vr, Ar>> for TArr<Vl, Al>
where
    Vl: Sub<Vr>,
    Al: Sub<Ar>,
{
    type Output = TArr<Diff<Vl, Vr>, Diff<Al, Ar>>;
    fn sub(self, _: TArr<Vr, Ar>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Multiply an array by a scalar

impl<Rhs> Mul<Rhs> for ATerm {
    type Output = ATerm;
    fn mul(self, _: Rhs) -> Self::Output {
        ATerm
    }
}

impl<V, A, Rhs> Mul<Rhs> for TArr<V, A>
where
    V: Mul<Rhs>,
    A: Mul<Rhs>,
{
    type Output = TArr<Prod<V, Rhs>, Prod<A, Rhs>>;
    fn mul(self, _: Rhs) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

impl Mul<ATerm> for Z0 {
    type Output = ATerm;
    fn mul(self, _: ATerm) -> Self::Output {
        ATerm
    }
}

impl<U> Mul<ATerm> for PInt<U>
where
    U: Unsigned + NonZero,
{
    type Output = ATerm;
    fn mul(self, _: ATerm) -> Self::Output {
        ATerm
    }
}

impl<U> Mul<ATerm> for NInt<U>
where
    U: Unsigned + NonZero,
{
    type Output = ATerm;
    fn mul(self, _: ATerm) -> Self::Output {
        ATerm
    }
}

impl<V, A> Mul<TArr<V, A>> for Z0
where
    Z0: Mul<A>,
{
    type Output = TArr<Z0, Prod<Z0, A>>;
    fn mul(self, _: TArr<V, A>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

impl<V, A, U> Mul<TArr<V, A>> for PInt<U>
where
    U: Unsigned + NonZero,
    PInt<U>: Mul<A> + Mul<V>,
{
    type Output = TArr<Prod<PInt<U>, V>, Prod<PInt<U>, A>>;
    fn mul(self, _: TArr<V, A>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

impl<V, A, U> Mul<TArr<V, A>> for NInt<U>
where
    U: Unsigned + NonZero,
    NInt<U>: Mul<A> + Mul<V>,
{
    type Output = TArr<Prod<NInt<U>, V>, Prod<NInt<U>, A>>;
    fn mul(self, _: TArr<V, A>) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Divide an array by a scalar

impl<Rhs> Div<Rhs> for ATerm {
    type Output = ATerm;
    fn div(self, _: Rhs) -> Self::Output {
        ATerm
    }
}

impl<V, A, Rhs> Div<Rhs> for TArr<V, A>
where
    V: Div<Rhs>,
    A: Div<Rhs>,
{
    type Output = TArr<Quot<V, Rhs>, Quot<A, Rhs>>;
    fn div(self, _: Rhs) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Partial Divide an array by a scalar

impl<Rhs> PartialDiv<Rhs> for ATerm {
    type Output = ATerm;
    fn partial_div(self, _: Rhs) -> Self::Output {
        ATerm
    }
}

impl<V, A, Rhs> PartialDiv<Rhs> for TArr<V, A>
where
    V: PartialDiv<Rhs>,
    A: PartialDiv<Rhs>,
{
    type Output = TArr<PartialQuot<V, Rhs>, PartialQuot<A, Rhs>>;
    fn partial_div(self, _: Rhs) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Modulo an array by a scalar
use core::ops::Rem;

impl<Rhs> Rem<Rhs> for ATerm {
    type Output = ATerm;
    fn rem(self, _: Rhs) -> Self::Output {
        ATerm
    }
}

impl<V, A, Rhs> Rem<Rhs> for TArr<V, A>
where
    V: Rem<Rhs>,
    A: Rem<Rhs>,
{
    type Output = TArr<Mod<V, Rhs>, Mod<A, Rhs>>;
    fn rem(self, _: Rhs) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}

// ---------------------------------------------------------------------------------------
// Negate an array
use core::ops::Neg;

impl Neg for ATerm {
    type Output = ATerm;
    fn neg(self) -> Self::Output {
        ATerm
    }
}

impl<V, A> Neg for TArr<V, A>
where
    V: Neg,
    A: Neg,
{
    type Output = TArr<Negate<V>, Negate<A>>;
    fn neg(self) -> Self::Output {
        unsafe { ::core::mem::uninitialized() }
    }
}
