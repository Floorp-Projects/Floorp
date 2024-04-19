//! 256-bit wide vector types
#[rustfmt::skip]

use crate::*;

impl_i!([i8; 32]: i8x32, m8x32 | i8, u32 | test_v256 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31 |
        From: |
        /// A 256-bit vector with 32 `i8` lanes.
);
impl_u!([u8; 32]: u8x32, m8x32 | u8, u32 | test_v256 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31 |
        From: |
        /// A 256-bit vector with 32 `u8` lanes.
);
impl_m!([m8; 32]: m8x32 | i8, u32 | test_v256 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31 |
        From:  |
        /// A 256-bit vector mask with 32 `m8` lanes.
);

impl_i!([i16; 16]: i16x16, m16x16 | i16, u16 | test_v256 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 |
        From: i8x16, u8x16 |
        /// A 256-bit vector with 16 `i16` lanes.
);
impl_u!([u16; 16]: u16x16, m16x16 | u16, u16 | test_v256 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 |
        From: u8x16 |
        /// A 256-bit vector with 16 `u16` lanes.
);
impl_m!([m16; 16]: m16x16 | i16, u16 | test_v256 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 |
        From: m8x16 |
        /// A 256-bit vector mask with 16 `m16` lanes.
);

impl_i!([i32; 8]: i32x8, m32x8 | i32, u8 | test_v256 | x0, x1, x2, x3, x4, x5, x6, x7  |
        From: i8x8, u8x8, i16x8, u16x8 |
        /// A 256-bit vector with 8 `i32` lanes.
);
impl_u!([u32; 8]: u32x8, m32x8 | u32, u8 | test_v256 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: u8x8, u16x8 |
        /// A 256-bit vector with 8 `u32` lanes.
);
impl_f!([f32; 8]: f32x8, m32x8 | f32 | test_v256 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: i8x8, u8x8, i16x8, u16x8 |
        /// A 256-bit vector with 8 `f32` lanes.
);
impl_m!([m32; 8]: m32x8 | i32, u8 | test_v256 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: m8x8, m16x8 |
        /// A 256-bit vector mask with 8 `m32` lanes.
);

impl_i!([i64; 4]: i64x4, m64x4 | i64, u8 | test_v256 | x0, x1, x2, x3 |
        From: i8x4, u8x4, i16x4, u16x4, i32x4, u32x4 |
        /// A 256-bit vector with 4 `i64` lanes.
);
impl_u!([u64; 4]: u64x4, m64x4 | u64, u8 | test_v256 | x0, x1, x2, x3 |
        From: u8x4, u16x4, u32x4 |
        /// A 256-bit vector with 4 `u64` lanes.
);
impl_f!([f64; 4]: f64x4, m64x4 | f64 | test_v256 | x0, x1, x2, x3 |
        From: i8x4, u8x4, i16x4, u16x4, i32x4, u32x4, f32x4 |
        /// A 256-bit vector with 4 `f64` lanes.
);
impl_m!([m64; 4]: m64x4 | i64, u8 | test_v256 | x0, x1, x2, x3 |
        From: m8x4, m16x4, m32x4 |
        /// A 256-bit vector mask with 4 `m64` lanes.
);

impl_i!([i128; 2]: i128x2, m128x2 | i128, u8 | test_v256 | x0, x1 |
        From: i8x2, u8x2, i16x2, u16x2, i32x2, u32x2, i64x2, u64x2 |
        /// A 256-bit vector with 2 `i128` lanes.
);
impl_u!([u128; 2]: u128x2, m128x2 | u128, u8 | test_v256 | x0, x1 |
        From: u8x2, u16x2, u32x2, u64x2 |
        /// A 256-bit vector with 2 `u128` lanes.
);
impl_m!([m128; 2]: m128x2 | i128, u8 | test_v256 | x0, x1 |
        From: m8x2, m16x2, m32x2, m64x2 |
        /// A 256-bit vector mask with 2 `m128` lanes.
);
