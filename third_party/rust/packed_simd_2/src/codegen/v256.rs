//! Internal 256-bit wide vector types

use crate::masks::*;

#[rustfmt::skip]
impl_simd_array!(
    [i8; 32]: i8x32 |
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8
);
#[rustfmt::skip]
impl_simd_array!(
    [u8; 32]: u8x32 |
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8
);
#[rustfmt::skip]
impl_simd_array!(
    [m8; 32]: m8x32 |
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8
);
#[rustfmt::skip]
impl_simd_array!(
    [i16; 16]: i16x16 |
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16
);
#[rustfmt::skip]
impl_simd_array!(
    [u16; 16]: u16x16 |
    u16, u16, u16, u16,
    u16, u16, u16, u16,
    u16, u16, u16, u16,
    u16, u16, u16, u16
);
#[rustfmt::skip]
impl_simd_array!(
    [m16; 16]: m16x16 |
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16
);

impl_simd_array!([i32; 8]: i32x8 | i32, i32, i32, i32, i32, i32, i32, i32);
impl_simd_array!([u32; 8]: u32x8 | u32, u32, u32, u32, u32, u32, u32, u32);
impl_simd_array!([f32; 8]: f32x8 | f32, f32, f32, f32, f32, f32, f32, f32);
impl_simd_array!([m32; 8]: m32x8 | i32, i32, i32, i32, i32, i32, i32, i32);

impl_simd_array!([i64; 4]: i64x4 | i64, i64, i64, i64);
impl_simd_array!([u64; 4]: u64x4 | u64, u64, u64, u64);
impl_simd_array!([f64; 4]: f64x4 | f64, f64, f64, f64);
impl_simd_array!([m64; 4]: m64x4 | i64, i64, i64, i64);

impl_simd_array!([i128; 2]: i128x2 | i128, i128);
impl_simd_array!([u128; 2]: u128x2 | u128, u128);
impl_simd_array!([m128; 2]: m128x2 | i128, i128);
