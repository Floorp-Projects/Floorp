use std::{usize, u8, u16, u32, u64};
use std::{isize, i8, i16, i32, i64};
use std::{f32, f64};

/// Numbers which have upper and lower bounds
pub trait Bounded {
    // FIXME (#5527): These should be associated constants
    /// returns the smallest finite number this type can represent
    fn min_value() -> Self;
    /// returns the largest finite number this type can represent
    fn max_value() -> Self;
}

macro_rules! bounded_impl {
    ($t:ty, $min:expr, $max:expr) => {
        impl Bounded for $t {
            #[inline]
            fn min_value() -> $t { $min }

            #[inline]
            fn max_value() -> $t { $max }
        }
    }
}

bounded_impl!(usize, usize::MIN, usize::MAX);
bounded_impl!(u8,    u8::MIN,    u8::MAX);
bounded_impl!(u16,   u16::MIN,   u16::MAX);
bounded_impl!(u32,   u32::MIN,   u32::MAX);
bounded_impl!(u64,   u64::MIN,   u64::MAX);

bounded_impl!(isize, isize::MIN, isize::MAX);
bounded_impl!(i8,    i8::MIN,    i8::MAX);
bounded_impl!(i16,   i16::MIN,   i16::MAX);
bounded_impl!(i32,   i32::MIN,   i32::MAX);
bounded_impl!(i64,   i64::MIN,   i64::MAX);

bounded_impl!(f32, f32::MIN, f32::MAX);

macro_rules! for_each_tuple_ {
    ( $m:ident !! ) => (
        $m! { }
    );
    ( $m:ident !! $h:ident, $($t:ident,)* ) => (
        $m! { $h $($t)* }
        for_each_tuple_! { $m !! $($t,)* }
    );
}
macro_rules! for_each_tuple {
    ( $m:ident ) => (
        for_each_tuple_! { $m !! A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, }
    );
}

macro_rules! bounded_tuple {
    ( $($name:ident)* ) => (
        impl<$($name: Bounded,)*> Bounded for ($($name,)*) {
            #[inline]
            fn min_value() -> Self {
                ($($name::min_value(),)*)
            }
            #[inline]
            fn max_value() -> Self {
                ($($name::max_value(),)*)
            }
        }
    );
}

for_each_tuple!(bounded_tuple);
bounded_impl!(f64, f64::MIN, f64::MAX);
