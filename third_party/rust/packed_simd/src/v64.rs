//! 64-bit wide vector types
#[rustfmt::skip]

use super::*;

impl_i!([i8; 8]: i8x8, m8x8 | i8, u8 | test_v64 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: |
        /// A 64-bit vector with 8 `i8` lanes.
);
impl_u!([u8; 8]: u8x8, m8x8 | u8, u8 | test_v64 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: |
        /// A 64-bit vector with 8 `u8` lanes.
);
impl_m!([m8; 8]: m8x8 | i8, u8 | test_v64 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: m16x8, m32x8 |
        /// A 64-bit vector mask with 8 `m8` lanes.
);

impl_i!([i16; 4]: i16x4, m16x4 | i16, u8 | test_v64 | x0, x1, x2, x3 |
        From: i8x4, u8x4 |
        /// A 64-bit vector with 4 `i16` lanes.
);
impl_u!([u16; 4]: u16x4, m16x4 | u16, u8 | test_v64 | x0, x1, x2, x3 |
        From: u8x4 |
        /// A 64-bit vector with 4 `u16` lanes.
);
impl_m!([m16; 4]: m16x4 | i16, u8 | test_v64 | x0, x1, x2, x3 |
        From: m8x4, m32x4, m64x4 |
        /// A 64-bit vector mask with 4 `m16` lanes.
);

impl_i!([i32; 2]: i32x2, m32x2 | i32, u8 | test_v64 | x0, x1 |
        From: i8x2, u8x2, i16x2, u16x2 |
        /// A 64-bit vector with 2 `i32` lanes.
);
impl_u!([u32; 2]: u32x2, m32x2 | u32, u8 | test_v64 | x0, x1 |
        From: u8x2, u16x2 |
        /// A 64-bit vector with 2 `u32` lanes.
);
impl_m!([m32; 2]: m32x2 | i32, u8 | test_v64 | x0, x1 |
        From: m8x2, m16x2, m64x2, m128x2 |
        /// A 64-bit vector mask with 2 `m32` lanes.
);
impl_f!([f32; 2]: f32x2, m32x2 | f32 | test_v64 | x0, x1 |
        From: i8x2, u8x2, i16x2, u16x2 |
        /// A 64-bit vector with 2 `f32` lanes.
);

/*
impl_i!([i64; 1]: i64x1, m64x1 | i64, u8 | test_v64 | x0 |
        From: /*i8x1, u8x1, i16x1, u16x1, i32x1, u32x1*/ |  // FIXME: primitive to vector conversion
        /// A 64-bit vector with 1 `i64` lanes.
);
impl_u!([u64; 1]: u64x1, m64x1 | u64, u8 | test_v64 | x0 |
        From: /*u8x1, u16x1, u32x1*/ | // FIXME: primitive to vector conversion
        /// A 64-bit vector with 1 `u64` lanes.
);
impl_m!([m64; 1]: m64x1 | i64, u8 | test_v64 | x0 |
        From: /*m8x1, m16x1, m32x1, */ m128x1 | // FIXME: unary small vector types
        /// A 64-bit vector mask with 1 `m64` lanes.
);
impl_f!([f64; 1]: f64x1, m64x1 | f64 | test_v64 | x0 |
        From: /*i8x1, u8x1, i16x1, u16x1, i32x1, u32x1, f32x1*/ | // FIXME: unary small vector types
        /// A 64-bit vector with 1 `f64` lanes.
);
*/
