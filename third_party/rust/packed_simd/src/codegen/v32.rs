//! Internal 32-bit wide vector types

use crate::masks::*;

impl_simd_array!([i8; 4]: i8x4 | i8, i8, i8, i8);
impl_simd_array!([u8; 4]: u8x4 | u8, u8, u8, u8);
impl_simd_array!([m8; 4]: m8x4 | i8, i8, i8, i8);

impl_simd_array!([i16; 2]: i16x2 | i16, i16);
impl_simd_array!([u16; 2]: u16x2 | u16, u16);
impl_simd_array!([m16; 2]: m16x2 | i16, i16);
