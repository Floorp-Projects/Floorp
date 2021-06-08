//! this module implements try clone for primitive rust types

use super::TryClone;
use crate::TryReserveError;

macro_rules! impl_try_clone {
    ($($e: ty),*) => {
        $(impl TryClone for $e {
            #[inline(always)]
            fn try_clone(&self) -> Result<Self, TryReserveError>
            where
                Self: core::marker::Sized,
            {
                Ok(*self)
            }
        }
        )*
    }
}

impl_try_clone!(u8, u16, u32, u64, i8, i16, i32, i64, usize, isize, bool);

impl<T: TryClone> TryClone for Option<T> {
    #[inline]
    fn try_clone(&self) -> Result<Self, TryReserveError> {
        Ok(match self {
            Some(t) => Some(t.try_clone()?),
            None => None,
        })
    }
}
// impl<T: Copy> TryClone for T {
//     fn try_clone(&self) -> Result<Self, TryReserveError>
//     where
//         Self: core::marker::Sized,
//     {
//         Ok(*self)
//     }
// }
