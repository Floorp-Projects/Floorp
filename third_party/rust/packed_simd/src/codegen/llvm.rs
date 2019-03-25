//! LLVM's platform intrinsics
#![allow(dead_code)]

use crate::sealed::Shuffle;
#[allow(unused_imports)] // FIXME: spurious warning?
use crate::sealed::Simd;

// Shuffle intrinsics: expanded in users' crates, therefore public.
extern "platform-intrinsic" {
    // FIXME: Passing this intrinsics an `idx` array with an index that is
    // out-of-bounds will produce a monomorphization-time error.
    // https://github.com/rust-lang-nursery/packed_simd/issues/21
    pub fn simd_shuffle2<T, U>(x: T, y: T, idx: [u32; 2]) -> U
    where
        T: Simd,
        <T as Simd>::Element: Shuffle<[u32; 2], Output = U>;

    pub fn simd_shuffle4<T, U>(x: T, y: T, idx: [u32; 4]) -> U
    where
        T: Simd,
        <T as Simd>::Element: Shuffle<[u32; 4], Output = U>;

    pub fn simd_shuffle8<T, U>(x: T, y: T, idx: [u32; 8]) -> U
    where
        T: Simd,
        <T as Simd>::Element: Shuffle<[u32; 8], Output = U>;

    pub fn simd_shuffle16<T, U>(x: T, y: T, idx: [u32; 16]) -> U
    where
        T: Simd,
        <T as Simd>::Element: Shuffle<[u32; 16], Output = U>;

    pub fn simd_shuffle32<T, U>(x: T, y: T, idx: [u32; 32]) -> U
    where
        T: Simd,
        <T as Simd>::Element: Shuffle<[u32; 32], Output = U>;

    pub fn simd_shuffle64<T, U>(x: T, y: T, idx: [u32; 64]) -> U
    where
        T: Simd,
        <T as Simd>::Element: Shuffle<[u32; 64], Output = U>;
}

pub use self::simd_shuffle16 as __shuffle_vector16;
pub use self::simd_shuffle2 as __shuffle_vector2;
pub use self::simd_shuffle32 as __shuffle_vector32;
pub use self::simd_shuffle4 as __shuffle_vector4;
pub use self::simd_shuffle64 as __shuffle_vector64;
pub use self::simd_shuffle8 as __shuffle_vector8;

extern "platform-intrinsic" {
    crate fn simd_eq<T, U>(x: T, y: T) -> U;
    crate fn simd_ne<T, U>(x: T, y: T) -> U;
    crate fn simd_lt<T, U>(x: T, y: T) -> U;
    crate fn simd_le<T, U>(x: T, y: T) -> U;
    crate fn simd_gt<T, U>(x: T, y: T) -> U;
    crate fn simd_ge<T, U>(x: T, y: T) -> U;

    crate fn simd_insert<T, U>(x: T, idx: u32, val: U) -> T;
    crate fn simd_extract<T, U>(x: T, idx: u32) -> U;

    crate fn simd_cast<T, U>(x: T) -> U;

    crate fn simd_add<T>(x: T, y: T) -> T;
    crate fn simd_sub<T>(x: T, y: T) -> T;
    crate fn simd_mul<T>(x: T, y: T) -> T;
    crate fn simd_div<T>(x: T, y: T) -> T;
    crate fn simd_rem<T>(x: T, y: T) -> T;
    crate fn simd_shl<T>(x: T, y: T) -> T;
    crate fn simd_shr<T>(x: T, y: T) -> T;
    crate fn simd_and<T>(x: T, y: T) -> T;
    crate fn simd_or<T>(x: T, y: T) -> T;
    crate fn simd_xor<T>(x: T, y: T) -> T;

    crate fn simd_reduce_add_unordered<T, U>(x: T) -> U;
    crate fn simd_reduce_mul_unordered<T, U>(x: T) -> U;
    crate fn simd_reduce_add_ordered<T, U>(x: T, acc: U) -> U;
    crate fn simd_reduce_mul_ordered<T, U>(x: T, acc: U) -> U;
    crate fn simd_reduce_min<T, U>(x: T) -> U;
    crate fn simd_reduce_max<T, U>(x: T) -> U;
    crate fn simd_reduce_min_nanless<T, U>(x: T) -> U;
    crate fn simd_reduce_max_nanless<T, U>(x: T) -> U;
    crate fn simd_reduce_and<T, U>(x: T) -> U;
    crate fn simd_reduce_or<T, U>(x: T) -> U;
    crate fn simd_reduce_xor<T, U>(x: T) -> U;
    crate fn simd_reduce_all<T>(x: T) -> bool;
    crate fn simd_reduce_any<T>(x: T) -> bool;

    crate fn simd_select<M, T>(m: M, a: T, b: T) -> T;

    crate fn simd_fmin<T>(a: T, b: T) -> T;
    crate fn simd_fmax<T>(a: T, b: T) -> T;

    crate fn simd_fsqrt<T>(a: T) -> T;
    crate fn simd_fma<T>(a: T, b: T, c: T) -> T;

    crate fn simd_gather<T, P, M>(value: T, pointers: P, mask: M) -> T;
    crate fn simd_scatter<T, P, M>(value: T, pointers: P, mask: M);
}
