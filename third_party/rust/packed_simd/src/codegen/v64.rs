//! Internal 64-bit wide vector types

use crate::masks::*;

impl_simd_array!([i8; 8]: i8x8 | i8, i8, i8, i8, i8, i8, i8, i8);
impl_simd_array!([u8; 8]: u8x8 | u8, u8, u8, u8, u8, u8, u8, u8);
impl_simd_array!([m8; 8]: m8x8 | i8, i8, i8, i8, i8, i8, i8, i8);

impl_simd_array!([i16; 4]: i16x4 | i16, i16, i16, i16);
impl_simd_array!([u16; 4]: u16x4 | u16, u16, u16, u16);
impl_simd_array!([m16; 4]: m16x4 | i16, i16, i16, i16);

impl_simd_array!([i32; 2]: i32x2 | i32, i32);
impl_simd_array!([u32; 2]: u32x2 | u32, u32);
impl_simd_array!([f32; 2]: f32x2 | f32, f32);
impl_simd_array!([m32; 2]: m32x2 | i32, i32);

impl_simd_array!([i64; 1]: i64x1 | i64);
impl_simd_array!([u64; 1]: u64x1 | u64);
impl_simd_array!([f64; 1]: f64x1 | f64);
impl_simd_array!([m64; 1]: m64x1 | i64);
