//! 128-bit wide vector types
#![rustfmt::skip]

use crate::*;

impl_i!([i8; 16]: i8x16, m8x16 | i8 | test_v128 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 |
        From: |
        /// A 128-bit vector with 16 `i8` lanes.
);
impl_u!([u8; 16]: u8x16, m8x16 | u8 | test_v128 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 |
        From: |
        /// A 128-bit vector with 16 `u8` lanes.
);
impl_m!([m8; 16]: m8x16 | i8 | test_v128 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 |
        From: m16x16 |
        /// A 128-bit vector mask with 16 `m8` lanes.
);

impl_i!([i16; 8]: i16x8, m16x8 | i16 | test_v128 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: i8x8, u8x8 |
        /// A 128-bit vector with 8 `i16` lanes.
);
impl_u!([u16; 8]: u16x8, m16x8 | u16| test_v128 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: u8x8 |
        /// A 128-bit vector with 8 `u16` lanes.
);
impl_m!([m16; 8]: m16x8 | i16 | test_v128 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: m8x8, m32x8 |
        /// A 128-bit vector mask with 8 `m16` lanes.
);

impl_i!([i32; 4]: i32x4, m32x4 | i32 | test_v128 | x0, x1, x2, x3 |
        From: i8x4, u8x4, i16x4, u16x4  |
        /// A 128-bit vector with 4 `i32` lanes.
);
impl_u!([u32; 4]: u32x4, m32x4 | u32| test_v128 | x0, x1, x2, x3 |
        From: u8x4, u16x4 |
        /// A 128-bit vector with 4 `u32` lanes.
);
impl_f!([f32; 4]: f32x4, m32x4 | f32 | test_v128 | x0, x1, x2, x3 |
        From: i8x4, u8x4, i16x4, u16x4 |
        /// A 128-bit vector with 4 `f32` lanes.
);
impl_m!([m32; 4]: m32x4 | i32 | test_v128 | x0, x1, x2, x3 |
        From: m8x4, m16x4, m64x4 |
        /// A 128-bit vector mask with 4 `m32` lanes.
);

impl_i!([i64; 2]: i64x2, m64x2 | i64 | test_v128 | x0, x1 |
        From: i8x2, u8x2, i16x2, u16x2, i32x2, u32x2 |
        /// A 128-bit vector with 2 `i64` lanes.
);
impl_u!([u64; 2]: u64x2, m64x2 | u64 | test_v128 | x0, x1 |
        From: u8x2, u16x2, u32x2 |
        /// A 128-bit vector with 2 `u64` lanes.
);
impl_f!([f64; 2]: f64x2, m64x2 | f64 | test_v128 | x0, x1 |
        From: i8x2, u8x2, i16x2, u16x2, i32x2, u32x2, f32x2 |
        /// A 128-bit vector with 2 `f64` lanes.
);
impl_m!([m64; 2]: m64x2 | i64 | test_v128 | x0, x1 |
        From: m8x2, m16x2, m32x2, m128x2 |
        /// A 128-bit vector mask with 2 `m64` lanes.
);

impl_i!([i128; 1]: i128x1, m128x1 | i128 | test_v128 | x0 |
        From: /*i8x1, u8x1, i16x1, u16x1, i32x1, u32x1, i64x1, u64x1 */ | // FIXME: unary small vector types
        /// A 128-bit vector with 1 `i128` lane.
);
impl_u!([u128; 1]: u128x1, m128x1 | u128 | test_v128 | x0 |
        From: /*u8x1, u16x1, u32x1, u64x1 */ | // FIXME: unary small vector types
        /// A 128-bit vector with 1 `u128` lane.
);
impl_m!([m128; 1]: m128x1 | i128 | test_v128 | x0 |
        From: /*m8x1, m16x1, m32x1, m64x1 */ | // FIXME: unary small vector types
        /// A 128-bit vector mask with 1 `m128` lane.
);
