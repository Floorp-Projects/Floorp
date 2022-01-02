//! Internal 512-bit wide vector types

use crate::masks::*;

#[rustfmt::skip]
impl_simd_array!(
    [i8; 64]: i8x64 |
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,

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
    [u8; 64]: u8x64 |
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,
    u8, u8, u8, u8,

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
    [m8; 64]: m8x64 |
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,
    i8, i8, i8, i8,

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
    [i16; 32]: i16x32 |
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16
);
#[rustfmt::skip]
impl_simd_array!(
    [u16; 32]: u16x32 |
    u16, u16, u16, u16,
    u16, u16, u16, u16,
    u16, u16, u16, u16,
    u16, u16, u16, u16,
    u16, u16, u16, u16,
    u16, u16, u16, u16,
    u16, u16, u16, u16,
    u16, u16, u16, u16
);
#[rustfmt::skip]
impl_simd_array!(
    [m16; 32]: m16x32 |
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16,
    i16, i16, i16, i16
);

#[rustfmt::skip]
impl_simd_array!(
    [i32; 16]: i32x16 |
    i32, i32, i32, i32,
    i32, i32, i32, i32,
    i32, i32, i32, i32,
    i32, i32, i32, i32
);
#[rustfmt::skip]
impl_simd_array!(
    [u32; 16]: u32x16 |
    u32, u32, u32, u32,
    u32, u32, u32, u32,
    u32, u32, u32, u32,
    u32, u32, u32, u32
);
#[rustfmt::skip]
impl_simd_array!(
    [f32; 16]: f32x16 |
    f32, f32, f32, f32,
    f32, f32, f32, f32,
    f32, f32, f32, f32,
    f32, f32, f32, f32
);
#[rustfmt::skip]
impl_simd_array!(
    [m32; 16]: m32x16 |
    i32, i32, i32, i32,
    i32, i32, i32, i32,
    i32, i32, i32, i32,
    i32, i32, i32, i32
);

impl_simd_array!([i64; 8]: i64x8 | i64, i64, i64, i64, i64, i64, i64, i64);
impl_simd_array!([u64; 8]: u64x8 | u64, u64, u64, u64, u64, u64, u64, u64);
impl_simd_array!([f64; 8]: f64x8 | f64, f64, f64, f64, f64, f64, f64, f64);
impl_simd_array!([m64; 8]: m64x8 | i64, i64, i64, i64, i64, i64, i64, i64);

impl_simd_array!([i128; 4]: i128x4 | i128, i128, i128, i128);
impl_simd_array!([u128; 4]: u128x4 | u128, u128, u128, u128);
impl_simd_array!([m128; 4]: m128x4 | i128, i128, i128, i128);
