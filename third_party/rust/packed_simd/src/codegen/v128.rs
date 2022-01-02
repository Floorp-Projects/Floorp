//! Internal 128-bit wide vector types

use crate::masks::*;

#[rustfmt::skip]
impl_simd_array!(
    [i8; 16]: i8x16 |
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8
);
#[rustfmt::skip]
impl_simd_array!(
    [u8; 16]: u8x16 |
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8
);
#[rustfmt::skip]
impl_simd_array!(
    [m8; 16]: m8x16 |
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8
);

impl_simd_array!([i16; 8]: i16x8 | i16, i16, i16, i16, i16, i16, i16, i16);
impl_simd_array!([u16; 8]: u16x8 | u16, u16, u16, u16, u16, u16, u16, u16);
impl_simd_array!([m16; 8]: m16x8 | i16, i16, i16, i16, i16, i16, i16, i16);

impl_simd_array!([i32; 4]: i32x4 | i32, i32, i32, i32);
impl_simd_array!([u32; 4]: u32x4 | u32, u32, u32, u32);
impl_simd_array!([f32; 4]: f32x4 | f32, f32, f32, f32);
impl_simd_array!([m32; 4]: m32x4 | i32, i32, i32, i32);

impl_simd_array!([i64; 2]: i64x2 | i64, i64);
impl_simd_array!([u64; 2]: u64x2 | u64, u64);
impl_simd_array!([f64; 2]: f64x2 | f64, f64);
impl_simd_array!([m64; 2]: m64x2 | i64, i64);

impl_simd_array!([i128; 1]: i128x1 | i128);
impl_simd_array!([u128; 1]: u128x1 | u128);
impl_simd_array!([m128; 1]: m128x1 | i128);
