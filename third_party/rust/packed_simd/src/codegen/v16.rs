//! Internal 16-bit wide vector types

use crate::masks::*;

impl_simd_array!([i8; 2]: i8x2 | i8, i8);
impl_simd_array!([u8; 2]: u8x2 | u8, u8);
impl_simd_array!([m8; 2]: m8x2 | i8, i8);
