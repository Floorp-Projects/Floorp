//! Implementations of the `ShuffleResult` trait for the different numbers of
//! lanes and vector element types.

use crate::masks::*;
use crate::sealed::Shuffle;

impl Shuffle<[u32; 2]> for i8 {
    type Output = crate::codegen::i8x2;
}
impl Shuffle<[u32; 4]> for i8 {
    type Output = crate::codegen::i8x4;
}
impl Shuffle<[u32; 8]> for i8 {
    type Output = crate::codegen::i8x8;
}
impl Shuffle<[u32; 16]> for i8 {
    type Output = crate::codegen::i8x16;
}
impl Shuffle<[u32; 32]> for i8 {
    type Output = crate::codegen::i8x32;
}
impl Shuffle<[u32; 64]> for i8 {
    type Output = crate::codegen::i8x64;
}

impl Shuffle<[u32; 2]> for u8 {
    type Output = crate::codegen::u8x2;
}
impl Shuffle<[u32; 4]> for u8 {
    type Output = crate::codegen::u8x4;
}
impl Shuffle<[u32; 8]> for u8 {
    type Output = crate::codegen::u8x8;
}
impl Shuffle<[u32; 16]> for u8 {
    type Output = crate::codegen::u8x16;
}
impl Shuffle<[u32; 32]> for u8 {
    type Output = crate::codegen::u8x32;
}
impl Shuffle<[u32; 64]> for u8 {
    type Output = crate::codegen::u8x64;
}

impl Shuffle<[u32; 2]> for m8 {
    type Output = crate::codegen::m8x2;
}
impl Shuffle<[u32; 4]> for m8 {
    type Output = crate::codegen::m8x4;
}
impl Shuffle<[u32; 8]> for m8 {
    type Output = crate::codegen::m8x8;
}
impl Shuffle<[u32; 16]> for m8 {
    type Output = crate::codegen::m8x16;
}
impl Shuffle<[u32; 32]> for m8 {
    type Output = crate::codegen::m8x32;
}
impl Shuffle<[u32; 64]> for m8 {
    type Output = crate::codegen::m8x64;
}

impl Shuffle<[u32; 2]> for i16 {
    type Output = crate::codegen::i16x2;
}
impl Shuffle<[u32; 4]> for i16 {
    type Output = crate::codegen::i16x4;
}
impl Shuffle<[u32; 8]> for i16 {
    type Output = crate::codegen::i16x8;
}
impl Shuffle<[u32; 16]> for i16 {
    type Output = crate::codegen::i16x16;
}
impl Shuffle<[u32; 32]> for i16 {
    type Output = crate::codegen::i16x32;
}

impl Shuffle<[u32; 2]> for u16 {
    type Output = crate::codegen::u16x2;
}
impl Shuffle<[u32; 4]> for u16 {
    type Output = crate::codegen::u16x4;
}
impl Shuffle<[u32; 8]> for u16 {
    type Output = crate::codegen::u16x8;
}
impl Shuffle<[u32; 16]> for u16 {
    type Output = crate::codegen::u16x16;
}
impl Shuffle<[u32; 32]> for u16 {
    type Output = crate::codegen::u16x32;
}

impl Shuffle<[u32; 2]> for m16 {
    type Output = crate::codegen::m16x2;
}
impl Shuffle<[u32; 4]> for m16 {
    type Output = crate::codegen::m16x4;
}
impl Shuffle<[u32; 8]> for m16 {
    type Output = crate::codegen::m16x8;
}
impl Shuffle<[u32; 16]> for m16 {
    type Output = crate::codegen::m16x16;
}
impl Shuffle<[u32; 32]> for m16 {
    type Output = crate::codegen::m16x32;
}

impl Shuffle<[u32; 2]> for i32 {
    type Output = crate::codegen::i32x2;
}
impl Shuffle<[u32; 4]> for i32 {
    type Output = crate::codegen::i32x4;
}
impl Shuffle<[u32; 8]> for i32 {
    type Output = crate::codegen::i32x8;
}
impl Shuffle<[u32; 16]> for i32 {
    type Output = crate::codegen::i32x16;
}

impl Shuffle<[u32; 2]> for u32 {
    type Output = crate::codegen::u32x2;
}
impl Shuffle<[u32; 4]> for u32 {
    type Output = crate::codegen::u32x4;
}
impl Shuffle<[u32; 8]> for u32 {
    type Output = crate::codegen::u32x8;
}
impl Shuffle<[u32; 16]> for u32 {
    type Output = crate::codegen::u32x16;
}

impl Shuffle<[u32; 2]> for f32 {
    type Output = crate::codegen::f32x2;
}
impl Shuffle<[u32; 4]> for f32 {
    type Output = crate::codegen::f32x4;
}
impl Shuffle<[u32; 8]> for f32 {
    type Output = crate::codegen::f32x8;
}
impl Shuffle<[u32; 16]> for f32 {
    type Output = crate::codegen::f32x16;
}

impl Shuffle<[u32; 2]> for m32 {
    type Output = crate::codegen::m32x2;
}
impl Shuffle<[u32; 4]> for m32 {
    type Output = crate::codegen::m32x4;
}
impl Shuffle<[u32; 8]> for m32 {
    type Output = crate::codegen::m32x8;
}
impl Shuffle<[u32; 16]> for m32 {
    type Output = crate::codegen::m32x16;
}

/* FIXME: 64-bit single element vector
impl Shuffle<[u32; 1]> for i64 {
    type Output = crate::codegen::i64x1;
}
*/
impl Shuffle<[u32; 2]> for i64 {
    type Output = crate::codegen::i64x2;
}
impl Shuffle<[u32; 4]> for i64 {
    type Output = crate::codegen::i64x4;
}
impl Shuffle<[u32; 8]> for i64 {
    type Output = crate::codegen::i64x8;
}

/* FIXME: 64-bit single element vector
impl Shuffle<[u32; 1]> for u64 {
    type Output = crate::codegen::u64x1;
}
*/
impl Shuffle<[u32; 2]> for u64 {
    type Output = crate::codegen::u64x2;
}
impl Shuffle<[u32; 4]> for u64 {
    type Output = crate::codegen::u64x4;
}
impl Shuffle<[u32; 8]> for u64 {
    type Output = crate::codegen::u64x8;
}

/* FIXME: 64-bit single element vector
impl Shuffle<[u32; 1]> for f64 {
    type Output = crate::codegen::f64x1;
}
*/
impl Shuffle<[u32; 2]> for f64 {
    type Output = crate::codegen::f64x2;
}
impl Shuffle<[u32; 4]> for f64 {
    type Output = crate::codegen::f64x4;
}
impl Shuffle<[u32; 8]> for f64 {
    type Output = crate::codegen::f64x8;
}

/* FIXME: 64-bit single element vector
impl Shuffle<[u32; 1]> for m64 {
    type Output = crate::codegen::m64x1;
}
*/
impl Shuffle<[u32; 2]> for m64 {
    type Output = crate::codegen::m64x2;
}
impl Shuffle<[u32; 4]> for m64 {
    type Output = crate::codegen::m64x4;
}
impl Shuffle<[u32; 8]> for m64 {
    type Output = crate::codegen::m64x8;
}

impl Shuffle<[u32; 2]> for isize {
    type Output = crate::codegen::isizex2;
}
impl Shuffle<[u32; 4]> for isize {
    type Output = crate::codegen::isizex4;
}
impl Shuffle<[u32; 8]> for isize {
    type Output = crate::codegen::isizex8;
}

impl Shuffle<[u32; 2]> for usize {
    type Output = crate::codegen::usizex2;
}
impl Shuffle<[u32; 4]> for usize {
    type Output = crate::codegen::usizex4;
}
impl Shuffle<[u32; 8]> for usize {
    type Output = crate::codegen::usizex8;
}

impl<T> Shuffle<[u32; 2]> for *const T {
    type Output = crate::codegen::cptrx2<T>;
}
impl<T> Shuffle<[u32; 4]> for *const T {
    type Output = crate::codegen::cptrx4<T>;
}
impl<T> Shuffle<[u32; 8]> for *const T {
    type Output = crate::codegen::cptrx8<T>;
}

impl<T> Shuffle<[u32; 2]> for *mut T {
    type Output = crate::codegen::mptrx2<T>;
}
impl<T> Shuffle<[u32; 4]> for *mut T {
    type Output = crate::codegen::mptrx4<T>;
}
impl<T> Shuffle<[u32; 8]> for *mut T {
    type Output = crate::codegen::mptrx8<T>;
}

impl Shuffle<[u32; 2]> for msize {
    type Output = crate::codegen::msizex2;
}
impl Shuffle<[u32; 4]> for msize {
    type Output = crate::codegen::msizex4;
}
impl Shuffle<[u32; 8]> for msize {
    type Output = crate::codegen::msizex8;
}

impl Shuffle<[u32; 1]> for i128 {
    type Output = crate::codegen::i128x1;
}
impl Shuffle<[u32; 2]> for i128 {
    type Output = crate::codegen::i128x2;
}
impl Shuffle<[u32; 4]> for i128 {
    type Output = crate::codegen::i128x4;
}

impl Shuffle<[u32; 1]> for u128 {
    type Output = crate::codegen::u128x1;
}
impl Shuffle<[u32; 2]> for u128 {
    type Output = crate::codegen::u128x2;
}
impl Shuffle<[u32; 4]> for u128 {
    type Output = crate::codegen::u128x4;
}

impl Shuffle<[u32; 1]> for m128 {
    type Output = crate::codegen::m128x1;
}
impl Shuffle<[u32; 2]> for m128 {
    type Output = crate::codegen::m128x2;
}
impl Shuffle<[u32; 4]> for m128 {
    type Output = crate::codegen::m128x4;
}
