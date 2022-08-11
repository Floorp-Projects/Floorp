//! LLVM's platform intrinsics
#![allow(dead_code)]

use crate::sealed::Shuffle;
#[allow(unused_imports)] // FIXME: spurious warning?
use crate::sealed::Simd;

// Shuffle intrinsics: expanded in users' crates, therefore public.
extern "platform-intrinsic" {
    pub fn simd_shuffle2<T, U>(x: T, y: T, idx: [u32; 2]) -> U;
    pub fn simd_shuffle4<T, U>(x: T, y: T, idx: [u32; 4]) -> U;
    pub fn simd_shuffle8<T, U>(x: T, y: T, idx: [u32; 8]) -> U;
    pub fn simd_shuffle16<T, U>(x: T, y: T, idx: [u32; 16]) -> U;
    pub fn simd_shuffle32<T, U>(x: T, y: T, idx: [u32; 32]) -> U;
    pub fn simd_shuffle64<T, U>(x: T, y: T, idx: [u32; 64]) -> U;
}

#[allow(clippy::missing_safety_doc)]
#[inline]
pub unsafe fn __shuffle_vector2<const IDX: [u32; 2], T, U>(x: T, y: T) -> U
where
    T: Simd,
    <T as Simd>::Element: Shuffle<[u32; 2], Output = U>,
{
    simd_shuffle2(x, y, IDX)
}

#[allow(clippy::missing_safety_doc)]
#[inline]
pub unsafe fn __shuffle_vector4<const IDX: [u32; 4], T, U>(x: T, y: T) -> U
where
    T: Simd,
    <T as Simd>::Element: Shuffle<[u32; 4], Output = U>,
{
    simd_shuffle4(x, y, IDX)
}

#[allow(clippy::missing_safety_doc)]
#[inline]
pub unsafe fn __shuffle_vector8<const IDX: [u32; 8], T, U>(x: T, y: T) -> U
where
    T: Simd,
    <T as Simd>::Element: Shuffle<[u32; 8], Output = U>,
{
    simd_shuffle8(x, y, IDX)
}

#[allow(clippy::missing_safety_doc)]
#[inline]
pub unsafe fn __shuffle_vector16<const IDX: [u32; 16], T, U>(x: T, y: T) -> U
where
    T: Simd,
    <T as Simd>::Element: Shuffle<[u32; 16], Output = U>,
{
    simd_shuffle16(x, y, IDX)
}

#[allow(clippy::missing_safety_doc)]
#[inline]
pub unsafe fn __shuffle_vector32<const IDX: [u32; 32], T, U>(x: T, y: T) -> U
where
    T: Simd,
    <T as Simd>::Element: Shuffle<[u32; 32], Output = U>,
{
    simd_shuffle32(x, y, IDX)
}

#[allow(clippy::missing_safety_doc)]
#[inline]
pub unsafe fn __shuffle_vector64<const IDX: [u32; 64], T, U>(x: T, y: T) -> U
where
    T: Simd,
    <T as Simd>::Element: Shuffle<[u32; 64], Output = U>,
{
    simd_shuffle64(x, y, IDX)
}

extern "platform-intrinsic" {
    pub(crate) fn simd_eq<T, U>(x: T, y: T) -> U;
    pub(crate) fn simd_ne<T, U>(x: T, y: T) -> U;
    pub(crate) fn simd_lt<T, U>(x: T, y: T) -> U;
    pub(crate) fn simd_le<T, U>(x: T, y: T) -> U;
    pub(crate) fn simd_gt<T, U>(x: T, y: T) -> U;
    pub(crate) fn simd_ge<T, U>(x: T, y: T) -> U;

    pub(crate) fn simd_insert<T, U>(x: T, idx: u32, val: U) -> T;
    pub(crate) fn simd_extract<T, U>(x: T, idx: u32) -> U;

    pub(crate) fn simd_cast<T, U>(x: T) -> U;

    pub(crate) fn simd_add<T>(x: T, y: T) -> T;
    pub(crate) fn simd_sub<T>(x: T, y: T) -> T;
    pub(crate) fn simd_mul<T>(x: T, y: T) -> T;
    pub(crate) fn simd_div<T>(x: T, y: T) -> T;
    pub(crate) fn simd_rem<T>(x: T, y: T) -> T;
    pub(crate) fn simd_shl<T>(x: T, y: T) -> T;
    pub(crate) fn simd_shr<T>(x: T, y: T) -> T;
    pub(crate) fn simd_and<T>(x: T, y: T) -> T;
    pub(crate) fn simd_or<T>(x: T, y: T) -> T;
    pub(crate) fn simd_xor<T>(x: T, y: T) -> T;

    pub(crate) fn simd_reduce_add_unordered<T, U>(x: T) -> U;
    pub(crate) fn simd_reduce_mul_unordered<T, U>(x: T) -> U;
    pub(crate) fn simd_reduce_add_ordered<T, U>(x: T, acc: U) -> U;
    pub(crate) fn simd_reduce_mul_ordered<T, U>(x: T, acc: U) -> U;
    pub(crate) fn simd_reduce_min<T, U>(x: T) -> U;
    pub(crate) fn simd_reduce_max<T, U>(x: T) -> U;
    pub(crate) fn simd_reduce_min_nanless<T, U>(x: T) -> U;
    pub(crate) fn simd_reduce_max_nanless<T, U>(x: T) -> U;
    pub(crate) fn simd_reduce_and<T, U>(x: T) -> U;
    pub(crate) fn simd_reduce_or<T, U>(x: T) -> U;
    pub(crate) fn simd_reduce_xor<T, U>(x: T) -> U;
    pub(crate) fn simd_reduce_all<T>(x: T) -> bool;
    pub(crate) fn simd_reduce_any<T>(x: T) -> bool;

    pub(crate) fn simd_select<M, T>(m: M, a: T, b: T) -> T;

    pub(crate) fn simd_fmin<T>(a: T, b: T) -> T;
    pub(crate) fn simd_fmax<T>(a: T, b: T) -> T;

    pub(crate) fn simd_fsqrt<T>(a: T) -> T;
    pub(crate) fn simd_fma<T>(a: T, b: T, c: T) -> T;

    pub(crate) fn simd_gather<T, P, M>(value: T, pointers: P, mask: M) -> T;
    pub(crate) fn simd_scatter<T, P, M>(value: T, pointers: P, mask: M);

    pub(crate) fn simd_bitmask<T, U>(value: T) -> U;
}
