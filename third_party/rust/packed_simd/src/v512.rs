//! 512-bit wide vector types
#[rustfmt::skip]

use crate::*;

impl_i!([i8; 64]: i8x64, m8x64 | i8, u64 | test_v512 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31,
        x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47,
        x48, x49, x50, x51, x52, x53, x54, x55, x56, x57, x58, x59, x60, x61, x62, x63 |
        From: |
        /// A 512-bit vector with 64 `i8` lanes.
);
impl_u!([u8; 64]: u8x64, m8x64 | u8, u64 | test_v512 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31,
        x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47,
        x48, x49, x50, x51, x52, x53, x54, x55, x56, x57, x58, x59, x60, x61, x62, x63 |
        From: |
        /// A 512-bit vector with 64 `u8` lanes.
);
impl_m!([m8; 64]: m8x64 | i8, u64 | test_v512 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31,
        x32, x33, x34, x35, x36, x37, x38, x39, x40, x41, x42, x43, x44, x45, x46, x47,
        x48, x49, x50, x51, x52, x53, x54, x55, x56, x57, x58, x59, x60, x61, x62, x63 |
        From:  |
        /// A 512-bit vector mask with 64 `m8` lanes.
);

impl_i!([i16; 32]: i16x32, m16x32 | i16, u32 | test_v512 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31 |
        From: i8x32, u8x32 |
        /// A 512-bit vector with 32 `i16` lanes.
);
impl_u!([u16; 32]: u16x32, m16x32 | u16, u32 | test_v512 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31 |
        From: u8x32 |
        /// A 512-bit vector with 32 `u16` lanes.
);
impl_m!([m16; 32]: m16x32 | i16, u32 | test_v512 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        x16, x17, x18, x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31 |
        From: m8x32 |
        /// A 512-bit vector mask with 32 `m16` lanes.
);

impl_i!([i32; 16]: i32x16, m32x16 | i32, u16 | test_v512 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 |
        From: i8x16, u8x16, i16x16, u16x16 |
        /// A 512-bit vector with 16 `i32` lanes.
);
impl_u!([u32; 16]: u32x16, m32x16 | u32, u16 | test_v512 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 |
        From: u8x16, u16x16 |
        /// A 512-bit vector with 16 `u32` lanes.
);
impl_f!([f32; 16]: f32x16, m32x16 | f32 | test_v512 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 |
        From: i8x16, u8x16, i16x16, u16x16 |
        /// A 512-bit vector with 16 `f32` lanes.
);
impl_m!([m32; 16]: m32x16 | i32, u16 | test_v512 |
        x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15 |
        From: m8x16, m16x16 |
        /// A 512-bit vector mask with 16 `m32` lanes.
);

impl_i!([i64; 8]: i64x8, m64x8 | i64, u8 | test_v512 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: i8x8, u8x8, i16x8, u16x8, i32x8, u32x8 |
        /// A 512-bit vector with 8 `i64` lanes.
);
impl_u!([u64; 8]: u64x8, m64x8 | u64, u8 | test_v512 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: u8x8, u16x8, u32x8 |
        /// A 512-bit vector with 8 `u64` lanes.
);
impl_f!([f64; 8]: f64x8, m64x8 | f64 | test_v512 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: i8x8, u8x8, i16x8, u16x8, i32x8, u32x8, f32x8 |
        /// A 512-bit vector with 8 `f64` lanes.
);
impl_m!([m64; 8]: m64x8 | i64, u8 | test_v512 | x0, x1, x2, x3, x4, x5, x6, x7 |
        From: m8x8, m16x8, m32x8 |
        /// A 512-bit vector mask with 8 `m64` lanes.
);

impl_i!([i128; 4]: i128x4, m128x4 | i128, u8 | test_v512 | x0, x1, x2, x3 |
        From: i8x4, u8x4, i16x4, u16x4, i32x4, u32x4, i64x4, u64x4 |
        /// A 512-bit vector with 4 `i128` lanes.
);
impl_u!([u128; 4]: u128x4, m128x4 | u128, u8 | test_v512 | x0, x1, x2, x3 |
        From: u8x4, u16x4, u32x4, u64x4 |
        /// A 512-bit vector with 4 `u128` lanes.
);
impl_m!([m128; 4]: m128x4 | i128, u8 | test_v512 | x0, x1, x2, x3 |
        From: m8x4, m16x4, m32x4, m64x4 |
        /// A 512-bit vector mask with 4 `m128` lanes.
);
