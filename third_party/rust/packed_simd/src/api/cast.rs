//! Implementation of `FromCast` and `IntoCast`.
#![allow(clippy::module_name_repetitions)]

/// Numeric cast from `T` to `Self`.
///
/// > Note: This is a temporary workaround until the conversion traits
/// specified > in [RFC2484] are implemented.
///
/// Numeric cast between vectors with the same number of lanes, such that:
///
/// * casting integer vectors whose lane types have the same size (e.g. `i32xN`
/// -> `u32xN`) is a **no-op**,
///
/// * casting from a larger integer to a smaller integer (e.g. `u32xN` ->
/// `u8xN`) will **truncate**,
///
/// * casting from a smaller integer to a larger integer   (e.g. `u8xN` ->
///   `u32xN`) will:
///    * **zero-extend** if the source is unsigned, or
///    * **sign-extend** if the source is signed,
///
/// * casting from a float to an integer will **round the float towards zero**,
///
/// * casting from an integer to float will produce the floating point
/// representation of the integer, **rounding to nearest, ties to even**,
///
/// * casting from an `f32` to an `f64` is perfect and lossless,
///
/// * casting from an `f64` to an `f32` **rounds to nearest, ties to even**.
///
/// [RFC2484]: https://github.com/rust-lang/rfcs/pull/2484
pub trait FromCast<T>: crate::marker::Sized {
    /// Numeric cast from `T` to `Self`.
    fn from_cast(_: T) -> Self;
}

/// Numeric cast from `Self` to `T`.
///
/// > Note: This is a temporary workaround until the conversion traits
/// specified > in [RFC2484] are implemented.
///
/// Numeric cast between vectors with the same number of lanes, such that:
///
/// * casting integer vectors whose lane types have the same size (e.g. `i32xN`
/// -> `u32xN`) is a **no-op**,
///
/// * casting from a larger integer to a smaller integer (e.g. `u32xN` ->
/// `u8xN`) will **truncate**,
///
/// * casting from a smaller integer to a larger integer   (e.g. `u8xN` ->
///   `u32xN`) will:
///    * **zero-extend** if the source is unsigned, or
///    * **sign-extend** if the source is signed,
///
/// * casting from a float to an integer will **round the float towards zero**,
///
/// * casting from an integer to float will produce the floating point
/// representation of the integer, **rounding to nearest, ties to even**,
///
/// * casting from an `f32` to an `f64` is perfect and lossless,
///
/// * casting from an `f64` to an `f32` **rounds to nearest, ties to even**.
///
/// [RFC2484]: https://github.com/rust-lang/rfcs/pull/2484
pub trait Cast<T>: crate::marker::Sized {
    /// Numeric cast from `self` to `T`.
    fn cast(self) -> T;
}

/// `FromCast` implies `Cast`.
impl<T, U> Cast<U> for T
where
    U: FromCast<T>,
{
    #[inline]
    fn cast(self) -> U {
        U::from_cast(self)
    }
}

/// `FromCast` and `Cast` are reflexive
impl<T> FromCast<T> for T {
    #[inline]
    fn from_cast(t: Self) -> Self {
        t
    }
}

#[macro_use]
mod macros;

mod v16;
pub use self::v16::*;

mod v32;
pub use self::v32::*;

mod v64;
pub use self::v64::*;

mod v128;
pub use self::v128::*;

mod v256;
pub use self::v256::*;

mod v512;
pub use self::v512::*;
