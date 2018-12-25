//! Implementation for `arr!` macro.

use super::ArrayLength;
use core::ops::Add;
use typenum::U1;

/// Helper trait for `arr!` macro
pub trait AddLength<T, N: ArrayLength<T>>: ArrayLength<T> {
    /// Resulting length
    type Output: ArrayLength<T>;
}

impl<T, N1, N2> AddLength<T, N2> for N1
where
    N1: ArrayLength<T> + Add<N2>,
    N2: ArrayLength<T>,
    <N1 as Add<N2>>::Output: ArrayLength<T>,
{
    type Output = <N1 as Add<N2>>::Output;
}

/// Helper type for `arr!` macro
pub type Inc<T, U> = <U as AddLength<T, U1>>::Output;

#[doc(hidden)]
#[macro_export]
macro_rules! arr_impl {
    ($T:ty; $N:ty, [$($x:expr),*], []) => ({
        unsafe { $crate::transmute::<_, $crate::GenericArray<$T, $N>>([$($x),*]) }
    });
    ($T:ty; $N:ty, [], [$x1:expr]) => (
        arr_impl!($T; $crate::arr::Inc<$T, $N>, [$x1 as $T], [])
    );
    ($T:ty; $N:ty, [], [$x1:expr, $($x:expr),+]) => (
        arr_impl!($T; $crate::arr::Inc<$T, $N>, [$x1 as $T], [$($x),+])
    );
    ($T:ty; $N:ty, [$($y:expr),+], [$x1:expr]) => (
        arr_impl!($T; $crate::arr::Inc<$T, $N>, [$($y),+, $x1 as $T], [])
    );
    ($T:ty; $N:ty, [$($y:expr),+], [$x1:expr, $($x:expr),+]) => (
        arr_impl!($T; $crate::arr::Inc<$T, $N>, [$($y),+, $x1 as $T], [$($x),+])
    );
}

/// Macro allowing for easy generation of Generic Arrays.
/// Example: `let test = arr![u32; 1, 2, 3];`
#[macro_export]
macro_rules! arr {
    ($T:ty; $(,)*) => ({
        unsafe { $crate::transmute::<[$T; 0], $crate::GenericArray<$T, $crate::typenum::U0>>([]) }
    });
    ($T:ty; $($x:expr),* $(,)*) => (
        arr_impl!($T; $crate::typenum::U0, [], [$($x),*])
    );
    ($($x:expr,)+) => (arr![$($x),*]);
    () => ("""Macro requires a type, e.g. `let array = arr![u32; 1, 2, 3];`")
}
