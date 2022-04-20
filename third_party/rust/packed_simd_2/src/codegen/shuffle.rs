//! Implementations of the `ShuffleResult` trait for the different numbers of
//! lanes and vector element types.

use crate::masks::*;
use crate::sealed::{Seal, Shuffle};

macro_rules! impl_shuffle {
    ($array:ty, $base:ty, $out:ty) => {
        impl Seal<$array> for $base {}
        impl Shuffle<$array> for $base {
            type Output = $out;
        }
    };
}

impl_shuffle! { [u32; 2], i8, crate::codegen::i8x2 }
impl_shuffle! { [u32; 4], i8, crate::codegen::i8x4 }
impl_shuffle! { [u32; 8], i8, crate::codegen::i8x8 }
impl_shuffle! { [u32; 16], i8, crate::codegen::i8x16 }
impl_shuffle! { [u32; 32], i8, crate::codegen::i8x32 }
impl_shuffle! { [u32; 64], i8, crate::codegen::i8x64 }

impl_shuffle! { [u32; 2], u8, crate::codegen::u8x2 }
impl_shuffle! { [u32; 4], u8, crate::codegen::u8x4 }
impl_shuffle! { [u32; 8], u8, crate::codegen::u8x8 }
impl_shuffle! { [u32; 16], u8, crate::codegen::u8x16 }
impl_shuffle! { [u32; 32], u8, crate::codegen::u8x32 }
impl_shuffle! { [u32; 64], u8, crate::codegen::u8x64 }

impl_shuffle! { [u32; 2], m8, crate::codegen::m8x2 }
impl_shuffle! { [u32; 4], m8, crate::codegen::m8x4 }
impl_shuffle! { [u32; 8], m8, crate::codegen::m8x8 }
impl_shuffle! { [u32; 16], m8, crate::codegen::m8x16 }
impl_shuffle! { [u32; 32], m8, crate::codegen::m8x32 }
impl_shuffle! { [u32; 64], m8, crate::codegen::m8x64 }

impl_shuffle! { [u32; 2], i16, crate::codegen::i16x2 }
impl_shuffle! { [u32; 4], i16, crate::codegen::i16x4 }
impl_shuffle! { [u32; 8], i16, crate::codegen::i16x8 }
impl_shuffle! { [u32; 16], i16, crate::codegen::i16x16 }
impl_shuffle! { [u32; 32], i16, crate::codegen::i16x32 }

impl_shuffle! { [u32; 2], u16, crate::codegen::u16x2 }
impl_shuffle! { [u32; 4], u16, crate::codegen::u16x4 }
impl_shuffle! { [u32; 8], u16, crate::codegen::u16x8 }
impl_shuffle! { [u32; 16], u16, crate::codegen::u16x16 }
impl_shuffle! { [u32; 32], u16, crate::codegen::u16x32 }

impl_shuffle! { [u32; 2], m16, crate::codegen::m16x2 }
impl_shuffle! { [u32; 4], m16, crate::codegen::m16x4 }
impl_shuffle! { [u32; 8], m16, crate::codegen::m16x8 }
impl_shuffle! { [u32; 16], m16, crate::codegen::m16x16 }

impl_shuffle! { [u32; 2], i32, crate::codegen::i32x2 }
impl_shuffle! { [u32; 4], i32, crate::codegen::i32x4 }
impl_shuffle! { [u32; 8], i32, crate::codegen::i32x8 }
impl_shuffle! { [u32; 16], i32, crate::codegen::i32x16 }

impl_shuffle! { [u32; 2], u32, crate::codegen::u32x2 }
impl_shuffle! { [u32; 4], u32, crate::codegen::u32x4 }
impl_shuffle! { [u32; 8], u32, crate::codegen::u32x8 }
impl_shuffle! { [u32; 16], u32, crate::codegen::u32x16 }

impl_shuffle! { [u32; 2], f32, crate::codegen::f32x2 }
impl_shuffle! { [u32; 4], f32, crate::codegen::f32x4 }
impl_shuffle! { [u32; 8], f32, crate::codegen::f32x8 }
impl_shuffle! { [u32; 16], f32, crate::codegen::f32x16 }

impl_shuffle! { [u32; 2], m32, crate::codegen::m32x2 }
impl_shuffle! { [u32; 4], m32, crate::codegen::m32x4 }
impl_shuffle! { [u32; 8], m32, crate::codegen::m32x8 }
impl_shuffle! { [u32; 16], m32, crate::codegen::m32x16 }

/* FIXME: 64-bit single element vector
impl_shuffle! { [u32; 1], i64, crate::codegen::i64x1 }
*/
impl_shuffle! { [u32; 2], i64, crate::codegen::i64x2 }
impl_shuffle! { [u32; 4], i64, crate::codegen::i64x4 }
impl_shuffle! { [u32; 8], i64, crate::codegen::i64x8 }

/* FIXME: 64-bit single element vector
impl_shuffle! { [u32; 1], i64, crate::codegen::i64x1 }
*/
impl_shuffle! { [u32; 2], u64, crate::codegen::u64x2 }
impl_shuffle! { [u32; 4], u64, crate::codegen::u64x4 }
impl_shuffle! { [u32; 8], u64, crate::codegen::u64x8 }

/* FIXME: 64-bit single element vector
impl_shuffle! { [u32; 1], i64, crate::codegen::i64x1 }
*/
impl_shuffle! { [u32; 2], f64, crate::codegen::f64x2 }
impl_shuffle! { [u32; 4], f64, crate::codegen::f64x4 }
impl_shuffle! { [u32; 8], f64, crate::codegen::f64x8 }

/* FIXME: 64-bit single element vector
impl_shuffle! { [u32; 1], i64, crate::codegen::i64x1 }
*/
impl_shuffle! { [u32; 2], m64, crate::codegen::m64x2 }
impl_shuffle! { [u32; 4], m64, crate::codegen::m64x4 }
impl_shuffle! { [u32; 8], m64, crate::codegen::m64x8 }

impl_shuffle! { [u32; 2], isize, crate::codegen::isizex2 }
impl_shuffle! { [u32; 4], isize, crate::codegen::isizex4 }
impl_shuffle! { [u32; 8], isize, crate::codegen::isizex8 }

impl_shuffle! { [u32; 2], usize, crate::codegen::usizex2 }
impl_shuffle! { [u32; 4], usize, crate::codegen::usizex4 }
impl_shuffle! { [u32; 8], usize, crate::codegen::usizex8 }

impl_shuffle! { [u32; 2], msize, crate::codegen::msizex2 }
impl_shuffle! { [u32; 4], msize, crate::codegen::msizex4 }
impl_shuffle! { [u32; 8], msize, crate::codegen::msizex8 }

impl<T> Seal<[u32; 2]> for *const T {}
impl<T> Shuffle<[u32; 2]> for *const T {
    type Output = crate::codegen::cptrx2<T>;
}
impl<T> Seal<[u32; 4]> for *const T {}
impl<T> Shuffle<[u32; 4]> for *const T {
    type Output = crate::codegen::cptrx4<T>;
}
impl<T> Seal<[u32; 8]> for *const T {}
impl<T> Shuffle<[u32; 8]> for *const T {
    type Output = crate::codegen::cptrx8<T>;
}

impl<T> Seal<[u32; 2]> for *mut T {}
impl<T> Shuffle<[u32; 2]> for *mut T {
    type Output = crate::codegen::mptrx2<T>;
}
impl<T> Seal<[u32; 4]> for *mut T {}
impl<T> Shuffle<[u32; 4]> for *mut T {
    type Output = crate::codegen::mptrx4<T>;
}
impl<T> Seal<[u32; 8]> for *mut T {}
impl<T> Shuffle<[u32; 8]> for *mut T {
    type Output = crate::codegen::mptrx8<T>;
}

impl_shuffle! { [u32; 1], i128, crate::codegen::i128x1 }
impl_shuffle! { [u32; 2], i128, crate::codegen::i128x2 }
impl_shuffle! { [u32; 4], i128, crate::codegen::i128x4 }

impl_shuffle! { [u32; 1], u128, crate::codegen::u128x1 }
impl_shuffle! { [u32; 2], u128, crate::codegen::u128x2 }
impl_shuffle! { [u32; 4], u128, crate::codegen::u128x4 }

impl_shuffle! { [u32; 1], m128, crate::codegen::m128x1 }
impl_shuffle! { [u32; 2], m128, crate::codegen::m128x2 }
impl_shuffle! { [u32; 4], m128, crate::codegen::m128x4 }
