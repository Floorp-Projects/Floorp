//! Implementation of `FromBits` and `IntoBits`.

/// Safe lossless bitwise conversion from `T` to `Self`.
pub trait FromBits<T>: crate::marker::Sized {
    /// Safe lossless bitwise transmute from `T` to `Self`.
    fn from_bits(t: T) -> Self;
}

/// Safe lossless bitwise conversion from `Self` to `T`.
pub trait IntoBits<T>: crate::marker::Sized {
    /// Safe lossless bitwise transmute from `self` to `T`.
    fn into_bits(self) -> T;
}

/// `FromBits` implies `IntoBits`.
impl<T, U> IntoBits<U> for T
where
    U: FromBits<T>,
{
    #[inline]
    fn into_bits(self) -> U {
        debug_assert!(
            crate::mem::size_of::<Self>() == crate::mem::size_of::<U>()
        );
        U::from_bits(self)
    }
}

/// `FromBits` and `IntoBits` are reflexive
impl<T> FromBits<T> for T {
    #[inline]
    fn from_bits(t: Self) -> Self {
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

mod arch_specific;
pub use self::arch_specific::*;
